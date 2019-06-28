/*----------------------------------------------------------------------
 *  file:        drvModbusAsyn.cpp                                     
 *----------------------------------------------------------------------
 * EPICS asyn driver support for Modbus protocol communication with PLCs
 * 
 * Mark Rivers, University of Chicago
 * Original Date March 3, 2007
 *
 * Based on the modtcp and plctcp code from Rolf Keitel of Triumf, with
 * work from Ivan So at NSLS.
 *-----------------------------------------------------------------------
 *
 */


/* ANSI C includes  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* EPICS includes */
#include <dbAccess.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsTime.h>
#include <epicsEndian.h>
#include <epicsExit.h>
#include <cantProceed.h>
#include <errlog.h>
#include <osiSock.h>
#include <iocsh.h>

/* Asyn includes */
#include "asynPortDriver.h"
#include "asynOctetSyncIO.h"
#include "asynCommonSyncIO.h"

#include <epicsExport.h>
#include "modbus.h"
#include "drvModbusAsyn.h"

/* Defined constants */

#define MAX_READ_WORDS       125        /* Modbus limit on number of words to read */
#define MAX_WRITE_WORDS      123        /* Modbus limit on number of words to write */
#define MODBUS_READ_TIMEOUT  2.0        /* Timeout for asynOctetSyncIO->writeRead */
                                        /* Note: this value actually has no effect, the real 
                                         * timeout is set in modbusInterposeConfig */

#define WAGO_ID_STRING      "Wago"      /* If the plcName parameter to drvModbusAsynConfigure contains
                                         * this substring then the driver will do the initial readback
                                         * for write operations and the readback for read/modify/write
                                         * operations at address plus 0x200. */
                                         
#define WAGO_OFFSET         0x200       /* The offset for readback operations on Wago devices */

/* MODBUS_READ_WRITE_MULTIPLE_REGISTERS (function 23) is not very common, and is problematic.  
 * It can use a different range of registers for reading and writing.
 * We handle this by defining 2 new function codes.  
 * These will use MODBUS_READ_WRITE_MULTIPLE_REGISTERS
 * but one is read-only and the other is write-only.
 * Each EPICS modbus driver will use one or the other, and hence will behave like 
 * READ_INPUT_REGISTERS or WRITE_MULTIPLE_REGISTERS. */
#define MODBUS_READ_INPUT_REGISTERS_F23      123
#define MODBUS_WRITE_MULTIPLE_REGISTERS_F23  223

typedef struct {
    modbusDataType_t dataType;
    const char *dataTypeString;
} modbusDataTypeStruct;

static modbusDataTypeStruct modbusDataTypes[MAX_MODBUS_DATA_TYPES] = {
    {dataTypeUInt16,        MODBUS_UINT16_STRING},    
    {dataTypeInt16SM,       MODBUS_INT16_SM_STRING},    
    {dataTypeBCDUnsigned,   MODBUS_BCD_UNSIGNED_STRING},    
    {dataTypeBCDSigned,     MODBUS_BCD_SIGNED_STRING},    
    {dataTypeInt16,         MODBUS_INT16_STRING},    
    {dataTypeInt32LE,       MODBUS_INT32_LE_STRING},    
    {dataTypeInt32BE,       MODBUS_INT32_BE_STRING},    
    {dataTypeFloat32LE,     MODBUS_FLOAT32_LE_STRING},    
    {dataTypeFloat32BE,     MODBUS_FLOAT32_BE_STRING},    
    {dataTypeFloat64LE,     MODBUS_FLOAT64_LE_STRING},    
    {dataTypeFloat64BE,     MODBUS_FLOAT64_BE_STRING},    
    {dataTypeStringHigh,    MODBUS_STRING_HIGH_STRING},    
    {dataTypeStringLow,     MODBUS_STRING_LOW_STRING},    
    {dataTypeStringHighLow, MODBUS_STRING_HIGH_LOW_STRING},    
    {dataTypeStringLowHigh, MODBUS_STRING_LOW_HIGH_STRING},    
};  


/* Local variable declarations */
static const char *driverName = "drvModbusAsyn";           /* String for asynPrint */

static void readPollerC(void *drvPvt);


/********************************************************************
**  global driver functions
*********************************************************************
*/

static void modbusExitCallback(void *pPvt) {
    drvModbusAsyn *pDriver = (drvModbusAsyn*)pPvt;
    pDriver->modbusExiting_ = true;
}

drvModbusAsyn::drvModbusAsyn(const char *portName, const char *octetPortName, 
                             int modbusSlave, int modbusFunction, 
                             int modbusStartAddress, int modbusLength,
                             modbusDataType_t dataType,
                             int pollMsec, 
                             const char *plcType)

   : asynPortDriver(portName, 
                    1, /* maxAddr */ 
                    asynInt32Mask | asynUInt32DigitalMask | asynFloat64Mask | asynInt32ArrayMask | asynOctetMask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynUInt32DigitalMask | asynFloat64Mask | asynInt32ArrayMask | asynOctetMask,                   /* Interrupt mask */
                    ASYN_CANBLOCK | ASYN_MULTIDEVICE, /* asynFlags */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0), /* Default stack size*/

    modbusExiting_(false),
    octetPortName_(epicsStrDup(octetPortName)),
    plcType_(NULL),
    isConnected_(false),
    ioStatus_(asynSuccess),
    modbusSlave_(modbusSlave),
    modbusFunction_(modbusFunction),
    modbusStartAddress_(modbusStartAddress),
    modbusLength_(modbusLength),
    absoluteAddressing_(false),
    dataType_(dataType),
    data_(0),
    pollDelay_(pollMsec/1000.),
    forceCallback_(false),
    readOnceFunction_(0),
    readOnceDone_(false),
    prevIOStatus_(asynSuccess),
    readOK_(0),
    writeOK_(0),
    IOErrors_(0),
    currentIOErrors_(0),
    maxIOMsec_(0),
    lastIOMsec_(0),
    enableHistogram_(false),
    histogramMsPerBin_(1),
    readbackOffset_(0)

{
    int status;
    char readThreadName[100];
    int needReadThread=0;
    int IOLength=0;
    int maxLength=0;
    static const char *functionName="drvModbusAsyn";

    if (plcType_ == NULL) plcType = "";
    plcType_ = epicsStrDup(plcType);
     if (modbusStartAddress == -1) {
        absoluteAddressing_ = true;
    }
    
    /* Set readback offset for Wago devices for which the register readback address 
     * is different from the register write address */
    if (strstr(plcType_, WAGO_ID_STRING) != NULL) 
        readbackOffset_ = WAGO_OFFSET;

    createParam(MODBUS_DATA_STRING,                 asynParamInt32,       &P_Data);
    createParam(MODBUS_READ_STRING,                 asynParamInt32,       &P_Read);
    createParam(MODBUS_ENABLE_HISTOGRAM_STRING,     asynParamUInt32Digital, &P_EnableHistogram);
    createParam(MODBUS_READ_HISTOGRAM_STRING,       asynParamInt32,       &P_ReadHistogram);
    createParam(MODBUS_HISTOGRAM_BIN_TIME_STRING,   asynParamInt32,       &P_HistogramBinTime);
    createParam(MODBUS_HISTOGRAM_TIME_AXIS_STRING,  asynParamInt32Array,  &P_HistogramTimeAxis);
    createParam(MODBUS_POLL_DELAY_STRING,           asynParamFloat64,     &P_PollDelay);
    createParam(MODBUS_READ_OK_STRING,              asynParamInt32,       &P_ReadOK);
    createParam(MODBUS_WRITE_OK_STRING,             asynParamInt32,       &P_WriteOK);
    createParam(MODBUS_IO_ERRORS_STRING,            asynParamInt32,       &P_IOErrors);
    createParam(MODBUS_LAST_IO_TIME_STRING,         asynParamInt32,       &P_LastIOTime);
    createParam(MODBUS_MAX_IO_TIME_STRING,          asynParamInt32,       &P_MaxIOTime);

    setIntegerParam(P_ReadOK, 0);
    setIntegerParam(P_WriteOK, 0);
    setIntegerParam(P_IOErrors, 0);
    setIntegerParam(P_LastIOTime, 0);
    setIntegerParam(P_MaxIOTime, 0);

    switch(modbusFunction_) {
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
            IOLength = modbusLength_/16;
            maxLength = MAX_READ_WORDS;
            needReadThread = 1;
            break;
        case MODBUS_READ_HOLDING_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS_F23:
            IOLength = modbusLength_;
            maxLength = MAX_READ_WORDS;
            needReadThread = 1;
            break;
        case MODBUS_WRITE_SINGLE_COIL:
        case MODBUS_WRITE_MULTIPLE_COILS:
            IOLength = modbusLength_/16;
            maxLength = MAX_WRITE_WORDS;
            readOnceFunction_ = MODBUS_READ_COILS;
            break;
       case MODBUS_WRITE_SINGLE_REGISTER:
       case MODBUS_WRITE_MULTIPLE_REGISTERS:
            IOLength = modbusLength_;
            maxLength = MAX_WRITE_WORDS;
            readOnceFunction_ = MODBUS_READ_HOLDING_REGISTERS;
            break;
       case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
            IOLength = modbusLength_;
            maxLength = MAX_WRITE_WORDS;
            readOnceFunction_ = MODBUS_READ_INPUT_REGISTERS_F23;
            break;
       default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s port %s unsupported Modbus function %d\n",
                driverName, functionName, this->portName, modbusFunction_);
     }
 
    /* If we are using absolute addressing then don't start read thread */
    if (absoluteAddressing_) needReadThread = 0;

    /* Make sure memory length is valid. */
    if (modbusLength_ <= 0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s, port %s memory length<=0\n",
            driverName, functionName, this->portName);
    }
    if (IOLength > maxLength) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s, port %s memory length=%d too large, max=%d\n",
            driverName, functionName, this->portName, IOLength, maxLength);
    }
    
    /* Note that we always allocate modbusLength words of memory.  
     * This is needed even for write operations because we need a buffer to convert
     * data for asynInt32Array writes. */
    data_ = (epicsUInt16 *) callocMustSucceed(modbusLength_, sizeof(epicsUInt16), functionName);

    /* Connect to asyn octet port with asynOctetSyncIO */
    status = pasynOctetSyncIO->connect(octetPortName, 0, &pasynUserOctet_, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s port %s can't connect to asynOctet on Octet server %s.\n",
            driverName, functionName, portName, octetPortName);
    }

    /* Connect to asyn octet port with asynCommonSyncIO */
    status = pasynCommonSyncIO->connect(octetPortName, 0, &pasynUserCommon_, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s port %s can't connect to asynCommon on Octet server %s.\n",
        driverName, functionName, portName, octetPortName);
     }

    /* If this is an output function do a readOnce operation if required. */
    if (readOnceFunction_ && !absoluteAddressing_ && (pollDelay_ != 0)) {
         status = doModbusIO(modbusSlave_, readOnceFunction_, 
                            (modbusStartAddress_ + readbackOffset_), 
                            data_, modbusLength_);
        if (status == asynSuccess) readOnceDone_ = true;
    }
    
    /* Create the epicsEvent to wake up the readPoller.  
     * We do this even if there is no poller. */
    readPollerEventId_ = epicsEventCreate(epicsEventEmpty);
     
    /* Create the thread to read registers if this is a read function code */
    if (needReadThread) {
        epicsSnprintf(readThreadName, 100, "%sRead", this->portName);
        readPollerThreadId_ = epicsThreadCreate(readThreadName,
           epicsThreadPriorityMedium,
           epicsThreadGetStackSize(epicsThreadStackSmall),
           (EPICSTHREADFUNC)readPollerC, 
           this);
        forceCallback_ = true;
    }

    epicsAtExit(modbusExitCallback, this);

}


/* asynDrvUser routines */
asynStatus drvModbusAsyn::drvUserCreate(asynUser *pasynUser,
                                        const char *drvInfo,
                                        const char **pptypeName, size_t *psize)
{
    int i;
    int offset;
    const char *pstring;
    static const char *functionName="drvUserCreate";
    
    /* We are passed a string that identifies this command.
     * Set dataType and/or pasynUser->reason based on this string */

    pasynUser->drvUser = &dataType_;
    for (i=0; i<MAX_MODBUS_DATA_TYPES; i++) {
        pstring = modbusDataTypes[i].dataTypeString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(offset)) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid offset %d\n",
                          driverName, functionName, this->portName, offset);
                return asynError;
            }
            pasynUser->drvUser = &modbusDataTypes[i].dataType;
            pasynUser->reason = P_Data;
            if (pptypeName) *pptypeName = epicsStrDup(MODBUS_DATA_STRING);
            if (psize) *psize = sizeof(MODBUS_DATA_STRING);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                      "%s::%s, port %s data type=%s\n", 
                      driverName, functionName, this->portName, pstring);
            return asynSuccess;
        }
    }
    
    // If we get to here we call the base class
    return asynPortDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);

}

/***********************/
/* asynCommon routines */
/***********************/

/* Connect */
asynStatus drvModbusAsyn::connect(asynUser *pasynUser)
{
    int offset;
  
    pasynManager->getAddr(pasynUser, &offset);
    if (offset < -1) return asynError;
    if (absoluteAddressing_) {
        if (offset > 65535) return asynError;
    } else {
        if (offset >= modbusLength_) return asynError;
    }
    pasynManager->exceptionConnect(pasynUser);
    return asynSuccess;
}

/* Report  parameters */
void drvModbusAsyn::report(FILE *fp, int details)
{
    fprintf(fp, "modbus port: %s\n", this->portName);
    if (details) {
        fprintf(fp, "    asynOctet server:   %s\n", octetPortName_);
        fprintf(fp, "    modbusSlave:        %d\n", modbusSlave_);
        fprintf(fp, "    modbusFunction:     %d\n", modbusFunction_);
        fprintf(fp, "    modbusStartAddress: 0%o\n", modbusStartAddress_);
        fprintf(fp, "    modbusLength:       0%o\n", modbusLength_);
        fprintf(fp, "    absoluteAddressing: %s\n", absoluteAddressing_ ? "true" : "false");
        fprintf(fp, "    plcType:            %s\n", plcType_);
        fprintf(fp, "    I/O errors:         %d\n", IOErrors_);
        fprintf(fp, "    Read OK:            %d\n", readOK_);
        fprintf(fp, "    Write OK:           %d\n", writeOK_);
        fprintf(fp, "    pollDelay:          %f\n", pollDelay_);
        fprintf(fp, "    Time for last I/O   %d msec\n", lastIOMsec_);
        fprintf(fp, "    Max. I/O time:      %d msec\n", maxIOMsec_);
        fprintf(fp, "    Time per hist. bin: %d msec\n", histogramMsPerBin_);
    }
    asynPortDriver::report(fp, details);
}

asynStatus drvModbusAsyn::getAddress(asynUser *pasynUser, int *address)
{
    // We use the asyn address for Modbus register addressing, but not for multiple addresses in asynPortDriver 
    *address = 0;
    return asynSuccess;
}


/* 
**  asynUInt32D support
*/
asynStatus drvModbusAsyn::readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask)
{
    int offset;
    int modbusFunction;
    static const char *functionName = "readUInt32D";

    if (pasynUser->reason == P_Data) {
        pasynManager->getAddr(pasynUser, &offset);
        if (checkOffset(offset)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s port %s invalid offset %d\n",
                      driverName, functionName, this->portName, offset);
            return asynError;
        }
        if (absoluteAddressing_) {
            /* If absolute addressing then there is no poller running */
            if (checkModbusFunction(&modbusFunction)) return asynError;
            ioStatus_ = doModbusIO(modbusSlave_, modbusFunction,
                                   offset, data_, modbusLength_);
            if (ioStatus_ != asynSuccess) return(ioStatus_);
            offset = 0;
            readOnceDone_ = true;
        } else {
            if (ioStatus_ != asynSuccess) return(ioStatus_);
        }
        *value = 0;
        switch(modbusFunction_) {
            case MODBUS_READ_COILS:
            case MODBUS_READ_DISCRETE_INPUTS:
            case MODBUS_READ_HOLDING_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS_F23:
                *value = data_[offset];
                if ((mask != 0 ) && (mask != 0xFFFF)) *value &= mask;
                break;
            case MODBUS_WRITE_SINGLE_COIL:
            case MODBUS_WRITE_MULTIPLE_COILS:
            case MODBUS_WRITE_SINGLE_REGISTER:
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                if (!readOnceDone_) return asynError;
                *value = data_[offset];
                if ((mask != 0 ) && (mask != 0xFFFF)) *value &= mask;
                break;
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                  "%s::%s port %s function=0x%x,"
                  " offset=0%o, mask=0x%x, value=0x%x\n",
                  driverName, functionName, this->portName, modbusFunction_, 
                  offset, mask, *value);
        return asynSuccess;
    }
    else {
        return asynPortDriver::readUInt32Digital(pasynUser, value, mask);
    }
}


asynStatus drvModbusAsyn::writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask)
{
    int offset;
    int modbusAddress;
    int i;
    epicsUInt16 data = value;
    asynStatus status;
    static const char *functionName = "writeUInt32D";
 
    if (pasynUser->reason == P_Data) {
        pasynManager->getAddr(pasynUser, &offset);
        if (checkOffset(offset)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s port %s invalid offset %d\n",
                      driverName, functionName, this->portName, offset);
            return asynError;
        }
        if (absoluteAddressing_) {
            modbusAddress = offset;
        } else {
            modbusAddress = modbusStartAddress_ + offset;
        }
        switch(modbusFunction_) {
            case MODBUS_WRITE_SINGLE_COIL:
                status = doModbusIO(modbusSlave_, modbusFunction_, 
                                    modbusAddress, &data, 1);
                if (status != asynSuccess) return(status);
                break;
            case MODBUS_WRITE_SINGLE_REGISTER:
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
                /* Do this as a read/modify/write if mask is not all 0 or all 1 */
                if ((mask == 0) || (mask == 0xFFFF)) {
                    status = doModbusIO(modbusSlave_, modbusFunction_,
                                        modbusAddress, &data, 1);
                } else {
                    status = doModbusIO(modbusSlave_, MODBUS_READ_HOLDING_REGISTERS,
                                        modbusAddress + readbackOffset_, &data, 1);
                    if (status != asynSuccess) return(status);
                    /* Set bits that are set in the value and set in the mask */
                    data |=  (value & mask);
                    /* Clear bits that are clear in the value and set in the mask */
                    data  &= (value | ~mask);
                    status = doModbusIO(modbusSlave_, modbusFunction_,
                                        modbusAddress, &data, 1);
                }
                if (status != asynSuccess) return(status);
                break;
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
              "%s::%s port %s function=0x%x,"
              " address=0%o, mask=0x%x, value=0x%x\n",
              driverName, functionName, this->portName, modbusFunction_, 
              modbusAddress, mask, data);
    }
    else if (pasynUser->reason == P_EnableHistogram) {
        if ((value != 0) && enableHistogram_ == 0) {
            /* We are turning on histogram enabling, erase existing data first */
            for (i=0; i<HISTOGRAM_LENGTH; i++) {
                timeHistogram_[i] = 0;
            }
        }
        enableHistogram_ = value ? true : false;
    }
    else {
        return asynPortDriver::writeUInt32Digital(pasynUser, value, mask);
    }
    return asynSuccess;
}


/* 
**  asynInt32 support
*/

asynStatus drvModbusAsyn::readInt32 (asynUser *pasynUser, epicsInt32 *value)
{
    modbusDataType_t dataType = getDataType(pasynUser);
    int offset;
    asynStatus status;
    int bufferLen;
    int modbusFunction;
    static const char *functionName = "readInt32";
    
    *value = 0;
    
    if (pasynUser->reason == P_Data) {
        pasynManager->getAddr(pasynUser, &offset);
        if (checkOffset(offset)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s port %s invalid offset %d\n",
                      driverName, functionName, this->portName, offset);
            return asynError;
        }
        if (absoluteAddressing_) {
            /* If absolute addressing then there is no poller running */
            if (checkModbusFunction(&modbusFunction)) return asynError;
            ioStatus_ = doModbusIO(modbusSlave_, modbusFunction,
                                        offset, data_, modbusLength_);
            if (ioStatus_ != asynSuccess) return(ioStatus_);
            offset = 0;
            readOnceDone_ = true;
        } else {
            if (ioStatus_ != asynSuccess) return(ioStatus_);
        }
        switch(modbusFunction_) {
            case MODBUS_READ_COILS:
            case MODBUS_READ_DISCRETE_INPUTS:
                *value = data_[offset];
                break;
            case MODBUS_READ_HOLDING_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS_F23:
                status = readPlcInt(dataType, offset, value, &bufferLen);
                if (status != asynSuccess) return status;
                break;
            case MODBUS_WRITE_SINGLE_COIL:
            case MODBUS_WRITE_MULTIPLE_COILS:
                if (!readOnceDone_) return asynError ;
                *value = data_[offset];
                break;
            case MODBUS_WRITE_SINGLE_REGISTER:
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                if (!readOnceDone_) return asynError;
                status = readPlcInt(dataType, offset, value, &bufferLen);
                if (status != asynSuccess) return status;
                break;
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
              "%s::%s port %s function=0x%x,"
              " offset=0%o, value=0x%x\n",
              driverName, functionName, this->portName, modbusFunction_, 
              offset, *value);
        return asynSuccess;
    }
    else {
        return asynPortDriver::readInt32(pasynUser, value);
    }
}


asynStatus drvModbusAsyn::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    modbusDataType_t dataType = getDataType(pasynUser);
    int offset;
    int modbusAddress;
    int function = pasynUser->reason;
    epicsUInt16 buffer[4];
    int bufferLen=0;
    int i;
    asynStatus status;
    static const char *functionName = "writeInt32";

    if (function == P_Data) {
        pasynManager->getAddr(pasynUser, &offset);
        if (checkOffset(offset)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s port %s invalid offset %d\n",
                      driverName, functionName, this->portName, offset);
            return asynError;
        }
        if (absoluteAddressing_) {
            modbusAddress = offset;
            offset = 0;
        } else {
            modbusAddress = modbusStartAddress_ + offset;
        }
        switch(modbusFunction_) {
            case MODBUS_WRITE_SINGLE_COIL:
                buffer[0] = value;
                status = doModbusIO(modbusSlave_, modbusFunction_,
                                    modbusAddress, buffer, 1);
                if (status != asynSuccess) return(status);
                break;
            case MODBUS_WRITE_SINGLE_REGISTER:
                status = writePlcInt(dataType, offset, value, buffer, &bufferLen);
                if (status != asynSuccess) return(status);
                for (i=0; i<bufferLen; i++) {
                    status = doModbusIO(modbusSlave_, modbusFunction_,
                                        modbusAddress+i, buffer+i, bufferLen);
                }
                if (status != asynSuccess) return(status);
                break;
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                status = writePlcInt(dataType, offset, value, buffer, &bufferLen);
                if (status != asynSuccess) return(status);
                status = doModbusIO(modbusSlave_, modbusFunction_,
                                    modbusAddress, buffer, bufferLen);
                if (status != asynSuccess) return(status);
                break;
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s port %s function=0x%x,"
            " modbusAddress=0%o, buffer[0]=0x%x, bufferLen=%d\n",
            driverName, functionName, this->portName, modbusFunction_, 
            modbusAddress, buffer[0], bufferLen);
    }
    else if (function == P_Read) {
        /* Read the data for this driver.  This can be used when the poller is disabled. */
        epicsEventSignal(readPollerEventId_);
    }
    else if (function == P_HistogramBinTime) {
        /* Set the time per histogram bin in ms */
        histogramMsPerBin_ = value;
        if (histogramMsPerBin_ < 1)histogramMsPerBin_ = 1;
        /* Since the time might have changed erase existing data */
        for (i=0; i<HISTOGRAM_LENGTH; i++) {
            timeHistogram_[i] = 0;
            histogramTimeAxis_[i] = i *histogramMsPerBin_;
        }
    }
    else {
        return asynPortDriver::writeInt32(pasynUser, value);
    }
    return asynSuccess;
}


/* 
**  asynFloat64 support
*/
asynStatus drvModbusAsyn::readFloat64 (asynUser *pasynUser, epicsFloat64 *value)
{
    modbusDataType_t dataType = getDataType(pasynUser);
    int offset;
    int bufferLen;
    int modbusFunction;
    asynStatus status = asynSuccess;
    static const char *functionName="readFloat64";
    
    *value = 0;
    
    if (pasynUser->reason == P_Data) {
        pasynManager->getAddr(pasynUser, &offset);
        if (checkOffset(offset)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s port %s invalid offset %d\n",
                      driverName, functionName, this->portName, offset);
            return asynError;
        }
        if (absoluteAddressing_) {
            /* If absolute addressing then there is no poller running */
            if (checkModbusFunction(&modbusFunction)) return asynError;
            ioStatus_ = doModbusIO(modbusSlave_, modbusFunction,
                                        offset, data_, modbusLength_);
            if (ioStatus_ != asynSuccess) return(ioStatus_);
            offset = 0;
            readOnceDone_ = true;
        } else {
            if (ioStatus_ != asynSuccess) return(ioStatus_);
        }
        switch(modbusFunction_) {
            case MODBUS_READ_COILS:
            case MODBUS_READ_DISCRETE_INPUTS:
                *value = data_[offset];
                 break;
            case MODBUS_READ_HOLDING_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS_F23:
                status = readPlcFloat(dataType, offset, value, &bufferLen);
                if (status != asynSuccess) return status;
                break;
            case MODBUS_WRITE_SINGLE_COIL:
            case MODBUS_WRITE_MULTIPLE_COILS:
                if (!readOnceDone_) return asynError;
                *value = data_[offset];
                break;
            case MODBUS_WRITE_SINGLE_REGISTER:
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                if (!readOnceDone_) return asynError;
                status = readPlcFloat(dataType, offset, value, &bufferLen);
                break;
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
              "%s::%s port %s function=0x%x,"
              " offset=0%o, value=%f, status=%d\n",
              driverName, functionName, this->portName, modbusFunction_, 
              offset, *value, status);
    }
    else {
        return asynPortDriver::readFloat64(pasynUser, value);
    }
    
    return asynSuccess;
}


asynStatus drvModbusAsyn::writeFloat64 (asynUser *pasynUser, epicsFloat64 value)
{
    modbusDataType_t dataType = getDataType(pasynUser);
    int offset;
    int modbusAddress;
    epicsUInt16 buffer[4];
    int bufferLen;
    int i;
    asynStatus status;
    static const char *functionName="writeFloat64";


    if (pasynUser->reason == P_Data) {
        pasynManager->getAddr(pasynUser, &offset);
        if (checkOffset(offset)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s port %s invalid offset %d\n",
                      driverName, functionName, this->portName, offset);
            return asynError;
        }
        if (absoluteAddressing_) {
            modbusAddress = offset;
            offset = 0;
        } else {
            modbusAddress = modbusStartAddress_ + offset;
        }
        switch(modbusFunction_) {
            case MODBUS_WRITE_SINGLE_COIL:
                buffer[0] = (epicsUInt16)value;
                status = doModbusIO(modbusSlave_, modbusFunction_,
                                     modbusAddress, buffer, 1);
                if (status != asynSuccess) return(status);
                break;
            case MODBUS_WRITE_SINGLE_REGISTER:
                status = writePlcFloat(dataType, offset, value, buffer, &bufferLen);
                for (i=0; i<bufferLen; i++) {
                    status = doModbusIO(modbusSlave_, modbusFunction_,
                                        modbusAddress+i, buffer+i, 1);
                    if (status != asynSuccess) return(status);
                }
                break;
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                status = writePlcFloat(dataType, offset, value, buffer, &bufferLen);
                status = doModbusIO(modbusSlave_, modbusFunction_,
                                    modbusAddress, buffer, bufferLen);
                if (status != asynSuccess) return(status);
                break;
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                  "%s::%s port %s function=0x%x,"
                  " modbusAddress=0%o, buffer[0]=0x%x\n",
                  driverName, functionName, this->portName, modbusFunction_, 
                  modbusAddress, buffer[0]);
    }
    else if (pasynUser->reason == P_PollDelay) {
        pollDelay_ = value;
        /* Send an event to the poller, because it might have a long poll time, or
         * not be polling at all */
        epicsEventSignal(readPollerEventId_);
    }
    return asynSuccess;
}



/* 
**  asynInt32Array support
*/
asynStatus drvModbusAsyn::readInt32Array (asynUser *pasynUser, epicsInt32 *data, size_t maxChans, size_t *nactual)
{
    modbusDataType_t dataType = getDataType(pasynUser);
    int function = pasynUser->reason;
    int offset;
    size_t i;
    int bufferLen;
    int modbusFunction;
    asynStatus status;
    static const char *functionName="readInt32Array";
    
    *nactual = 0;
    pasynManager->getAddr(pasynUser, &offset);
    if (function == P_Data) {
        if (absoluteAddressing_) {
            /* If absolute addressing then there is no poller running */
            if (checkModbusFunction(&modbusFunction)) return asynError;
            ioStatus_ = doModbusIO(modbusSlave_, modbusFunction,
                                   offset, data_, modbusLength_);
            if (ioStatus_ != asynSuccess) return(ioStatus_);
            offset = 0;
        } else {
            if (ioStatus_ != asynSuccess) return(ioStatus_);
        }
        switch(modbusFunction_) {
            case MODBUS_READ_COILS:
            case MODBUS_READ_DISCRETE_INPUTS:
                for (i=0; i<maxChans && offset<modbusLength_; i++) {
                    data[i] = data_[offset];
                    offset++;
                }
                break;
            case MODBUS_READ_HOLDING_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS_F23:
                for (i=0; i<maxChans && offset<modbusLength_; i++) {
                    status = readPlcInt(dataType, offset, &data[i], &bufferLen);
                    if (status) return status;
                    offset += bufferLen;
                }
                break;
                
            case MODBUS_WRITE_SINGLE_COIL:
            case MODBUS_WRITE_MULTIPLE_COILS:
                if (!readOnceDone_) return asynError;
                for (i=0; i<maxChans && offset<modbusLength_; i++) {
                    data[i] = data_[offset];
                    offset++;
                }
                break;
            case MODBUS_WRITE_SINGLE_REGISTER:
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                if (!readOnceDone_) return asynError;
                for (i=0; i<maxChans && offset<modbusLength_; i++) {
                    status = readPlcInt(dataType, offset, &data[i], &bufferLen);
                    if (status) return status;
                    offset += bufferLen;
                }
                break;
                
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                    (char *)data_, i*2, 
                    "%s::%sArray port %s, function=0x%x\n",
                    driverName, functionName, this->portName, modbusFunction_);
    }
    else if (function == P_ReadHistogram) {
        for (i=0; i<maxChans && i<HISTOGRAM_LENGTH; i++) {
            data[i] = timeHistogram_[i];
        }
    }
        
    else if (function == P_HistogramTimeAxis) {
        for (i=0; i<maxChans && i<HISTOGRAM_LENGTH; i++) {
            data[i] = histogramTimeAxis_[i];
        }
    }
    else {   
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s port %s invalid pasynUser->reason %d\n",
                  driverName, functionName, this->portName, pasynUser->reason);
        return asynError;
    }
    
    *nactual = i;
    return asynSuccess;
}


asynStatus drvModbusAsyn::writeInt32Array(asynUser *pasynUser, epicsInt32 *data, size_t maxChans)
{
    modbusDataType_t dataType = getDataType(pasynUser);
    int function = pasynUser->reason;
    int modbusAddress;
    epicsUInt16 *dataAddress;
    int nwrite=0;
    size_t i;
    int offset;
    int outIndex;
    int bufferLen;
    asynStatus status;
    static const char *functionName="writeInt32Array";

    pasynManager->getAddr(pasynUser, &offset);
    if (absoluteAddressing_) {
        modbusAddress = offset;
        dataAddress = data_;
        outIndex = 0;
    } else {
        modbusAddress = modbusStartAddress_ + offset;
        dataAddress = data_ + offset;
        outIndex = offset;
    }
    if (function == P_Data) {
        switch(modbusFunction_) {
            case MODBUS_WRITE_MULTIPLE_COILS:
                /* Need to copy data to local buffer to convert to epicsUInt16 */
                for (i=0; i<maxChans && outIndex<modbusLength_; i++) {
                    data_[outIndex] = data[i];
                    outIndex++;
                    nwrite++;
                }
                status = doModbusIO(modbusSlave_, modbusFunction_,
                                    modbusAddress, dataAddress, nwrite);
                if (status != asynSuccess) return(status);
                break;
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                for (i=0; i<maxChans && outIndex<modbusLength_; i++) {
                    status = writePlcInt(dataType, outIndex, data[i], &data_[outIndex], &bufferLen);
                    if (status != asynSuccess) return(status);
                    outIndex += bufferLen;
                    nwrite += bufferLen;
                }
                status = doModbusIO(modbusSlave_, modbusFunction_,
                                    modbusAddress, dataAddress, nwrite);
                if (status != asynSuccess) return(status);
                break;
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                    (char *)data_, nwrite*2, 
                    "%s::%s port %s, function=0x%x\n",
                    driverName, functionName, this->portName, modbusFunction_);
    }
    else {       
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s port %s invalid pasynUser->reason %d\n",
                  driverName, functionName, this->portName, pasynUser->reason);
        return asynError;
    }
    return asynSuccess;
}


/* 
**  asynOctet support
*/
asynStatus drvModbusAsyn::readOctet(asynUser *pasynUser, char *data, size_t maxChars, size_t *nactual, int *eomReason)
{
    modbusDataType_t dataType = getDataType(pasynUser);
    int function = pasynUser->reason;
    int offset;
    int bufferLen;
    int modbusFunction;
    static const char *functionName="readOctet";
    
    *nactual = 0;
    pasynManager->getAddr(pasynUser, &offset);
    if (function == P_Data) {
        if (absoluteAddressing_) {
            /* If absolute addressing then there is no poller running */
            if (checkModbusFunction(&modbusFunction)) return asynError;
            ioStatus_ = doModbusIO(modbusSlave_, modbusFunction,
                                   offset, data_, modbusLength_);
            if (ioStatus_ != asynSuccess) return(ioStatus_);
            offset = 0;
        } else {
            if (ioStatus_ != asynSuccess) return(ioStatus_);
        }
        switch(modbusFunction_) {
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                if (!readOnceDone_) return asynError;
            case MODBUS_READ_HOLDING_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS:
            case MODBUS_READ_INPUT_REGISTERS_F23:
                readPlcString(dataType, offset, data, maxChars, &bufferLen);
                *nactual = bufferLen;
                *eomReason = ASYN_EOM_CNT;
                break;
                
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                    (char *)data_, *nactual, 
                    "%s::%s port %s, function=0x%x\n",
                    driverName, functionName, this->portName, modbusFunction_);
    }
    else {       
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s port %s invalid pasynUser->reason %d\n",
                  driverName, functionName, this->portName, pasynUser->reason);
        return asynError;
    }

    return asynSuccess;
}


asynStatus drvModbusAsyn::writeOctet (asynUser *pasynUser, const char *data, size_t maxChars, size_t *nActual)
{
    modbusDataType_t dataType = getDataType(pasynUser);
    int function = pasynUser->reason;
    int modbusAddress;
    epicsUInt16 *dataAddress;
    int offset;
    int bufferLen;
    asynStatus status;
    static const char *functionName="writeOctet";

    pasynManager->getAddr(pasynUser, &offset);
    if (absoluteAddressing_) {
        modbusAddress = offset;
        dataAddress = data_;
    } else {
        modbusAddress = modbusStartAddress_ + offset;
        dataAddress = data_ + offset;
    }
    if (function == P_Data) {
        switch(modbusFunction_) {
            case MODBUS_WRITE_MULTIPLE_REGISTERS:
            case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                writePlcString(dataType, offset, data, maxChars, nActual, &bufferLen);
                if (bufferLen <= 0) break;
                status = doModbusIO(modbusSlave_, modbusFunction_,
                                    modbusAddress, dataAddress, bufferLen);
                if (status != asynSuccess) return(status);
                break;
            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid request for Modbus"
                          " function %d\n",
                          driverName, functionName, this->portName, modbusFunction_);
                return asynError;
        }
        asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                    (char *)data_, bufferLen*2, 
                    "%s::%s port %s, function=0x%x\n",
                    driverName, functionName, this->portName, modbusFunction_);
    }
    else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s port %s invalid pasynUser->reason %d\n",
                  driverName, functionName, this->portName, pasynUser->reason);
        return asynError;
    }
    return asynSuccess;
}


static void readPollerC(void *drvPvt)
{
    drvModbusAsyn *pPvt = (drvModbusAsyn *)drvPvt;
    
    pPvt->readPoller();
}


/*
****************************************************************************
** Poller thread for port reads
   One instance spawned per asyn port
****************************************************************************
*/

void drvModbusAsyn::readPoller()
{

    ELLLIST *pclientList;
    interruptNode *pnode;
    int offset;
    int bufferLen;
    int anyChanged;
    asynStatus prevIOStatus=asynSuccess;
    asynUser *pasynUser;
    int i;
    epicsUInt16 newValue, prevValue, mask;
    epicsUInt32 uInt32Value;
    epicsInt32 int32Value;
    epicsFloat64 float64Value;
    modbusDataType_t dataType;
    char stringBuffer[MAX_READ_WORDS * 2];
    epicsUInt16 *prevData;    /* Previous contents of memory buffer */
    epicsInt32 *int32Data;    /* Buffer used for asynInt32Array callbacks */
    static const char *functionName="readPoller";

    prevData = (epicsUInt16 *) callocMustSucceed(modbusLength_, sizeof(epicsUInt16), 
                                 "drvModbusAsyn::readPoller");
    int32Data = (epicsInt32 *) callocMustSucceed(modbusLength_, sizeof(epicsInt32), 
                                 "drvModbusAsyn::readPoller");
                            
    lock();

    /* Loop forever */    
    while (1)
    {
        /* Sleep for the poll delay or waiting for epicsEvent with the port unlocked */
        unlock();
        if (pollDelay_ > 0.0) {
            epicsEventWaitWithTimeout(readPollerEventId_, pollDelay_);
        } else {
            epicsEventWait(readPollerEventId_);
        }

        if (modbusExiting_) break;

        /* Lock the port.  It is important that the port be locked so other threads cannot access the pPlc
         * structure while the poller thread is running. */
        lock();
         
        /* Read the data */
        ioStatus_ = doModbusIO(modbusSlave_, modbusFunction_,
                               modbusStartAddress_, data_, modbusLength_);
        /* If we have an I/O error this time and the previous time, just try again */
        if (ioStatus_ != asynSuccess &&
            ioStatus_ == prevIOStatus) {
            epicsThreadSleep(1.0);
            continue;
        }

        /* If the I/O status has changed then force callbacks */
        if (ioStatus_ != prevIOStatus) forceCallback_ = true;
        
        /* See if any memory location has actually changed.  
         * If not, no need to do callbacks. */
        anyChanged = memcmp(data_, prevData, 
                            modbusLength_*sizeof(epicsUInt16));
 
        /* Don't start polling until EPICS interruptAccept flag is set, 
         * because it does callbacks to device support. */
        while (!interruptAccept) {
            unlock();
            epicsThreadSleep(0.1);
            lock();
        }

        /* Process callbacks to device support. */

        /* See if there are any asynUInt32Digital callbacks registered to be called
         * when data changes.  These callbacks only happen if the value has changed */
        if (forceCallback_ || anyChanged){
            pasynManager->interruptStart(asynStdInterfaces.uInt32DigitalInterruptPvt, &pclientList);
            pnode = (interruptNode *)ellFirst(pclientList);
            asynUInt32DigitalInterrupt *pUInt32D;
            while (pnode) {
                pUInt32D = (asynUInt32DigitalInterrupt *)pnode->drvPvt;
                pasynUser = pUInt32D->pasynUser;
                if (pasynUser->reason != P_Data) {
                    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                              "%s::%s port %s invalid pasynUser->reason %d\n",
                              driverName, functionName, this->portName, pasynUser->reason);
                    break;
                }
                pasynManager->getAddr(pasynUser, &offset);
                if (checkOffset(offset)) {
                    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                              "%s::%s port %s invalid offset %d, max=%d\n",
                              driverName, functionName, this->portName, offset, modbusLength_);
                    break;
                }
                mask = pUInt32D->mask;
                newValue = data_[offset];
                if ((mask != 0 ) && (mask != 0xFFFF)) newValue &= mask;
                prevValue = prevData[offset];
                if ((mask != 0 ) && (mask != 0xFFFF)) prevValue &= mask;
                /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
                pasynUser->auxStatus = ioStatus_;
                if (forceCallback_ || (newValue != prevValue)) {
                    uInt32Value = newValue;
                    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                              "%s::%s, calling client %p"
                              " mask=0x%x, callback=%p, data=0x%x\n",
                              driverName, functionName, pUInt32D, pUInt32D->mask, pUInt32D->callback, uInt32Value);
                    pUInt32D->callback(pUInt32D->userPvt, pasynUser, uInt32Value);
                }
                pnode = (interruptNode *)ellNext(&pnode->node);
            }
            pasynManager->interruptEnd(asynStdInterfaces.uInt32DigitalInterruptPvt);
        }
                
        /* See if there are any asynInt32 callbacks registered to be called. 
         * These are called even if the data has not changed, because we could be doing
         * ADC averaging */
        pasynManager->interruptStart(asynStdInterfaces.int32InterruptPvt, &pclientList);
        pnode = (interruptNode *)ellFirst(pclientList);
        while (pnode) {
            asynInt32Interrupt *pInt32;
            pInt32 = (asynInt32Interrupt *)pnode->drvPvt;
            pasynUser = pInt32->pasynUser;
            if (pasynUser->reason != P_Data) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid pasynUser->reason %d\n",
                          driverName, functionName, this->portName, pasynUser->reason);
                break;
            }
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(offset)) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid memory request %d, max=%d\n",
                          driverName, functionName, this->portName, offset, modbusLength_);
                break;
            }
            dataType = getDataType(pasynUser);
            readPlcInt(dataType, offset, &int32Value, &bufferLen);
            /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
            pasynUser->auxStatus = ioStatus_;
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s::%s, calling client %p"
                      "callback=%p, data=0x%x\n",
                      driverName, functionName, pInt32, pInt32->callback, int32Value);
            pInt32->callback(pInt32->userPvt, pasynUser,
                             int32Value);
            pnode = (interruptNode *)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(asynStdInterfaces.int32InterruptPvt);
 
        /* See if there are any asynFloat64 callbacks registered to be called.
         * These are called even if the data has not changed, because we could be doing
         * ADC averaging */
        pasynManager->interruptStart(asynStdInterfaces.float64InterruptPvt, &pclientList);
        pnode = (interruptNode *)ellFirst(pclientList);
        while (pnode) {
            asynFloat64Interrupt *pFloat64;
            pFloat64 = (asynFloat64Interrupt *)pnode->drvPvt;
            pasynUser = pFloat64->pasynUser;
            if (pasynUser->reason != P_Data) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid pasynUser->reason %d\n",
                          driverName, functionName, this->portName, pasynUser->reason);
                break;
            }
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(offset)) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s port %s invalid memory request %d, max=%d\n",
                          driverName, functionName, this->portName, offset, modbusLength_);
                break;
            }
            dataType = getDataType(pasynUser);
            readPlcFloat(dataType, offset, &float64Value, &bufferLen);
            /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
            pFloat64->pasynUser->auxStatus = ioStatus_;
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s::%s, calling client %p"
                      "callback=%p, data=%f\n",
                      driverName, functionName, pFloat64, pFloat64->callback, float64Value);
            pFloat64->callback(pFloat64->userPvt, pasynUser,
                               float64Value);
            pnode = (interruptNode *)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(asynStdInterfaces.float64InterruptPvt);
        
       
        /* See if there are any asynInt32Array callbacks registered to be called.
         * These are only called when data changes */
        if (forceCallback_ || anyChanged){
            pasynManager->interruptStart(asynStdInterfaces.int32ArrayInterruptPvt, &pclientList);
            pnode = (interruptNode *)ellFirst(pclientList);
            while (pnode) {
                asynInt32ArrayInterrupt *pInt32Array;
                pInt32Array = (asynInt32ArrayInterrupt *)pnode->drvPvt;
                pasynUser = pInt32Array->pasynUser;
                if (pasynUser->reason != P_Data) {
                    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                              "%s::%s port %s invalid pasynUser->reason %d\n",
                              driverName, functionName, this->portName, pasynUser->reason);
                    break;
                }
                /* Need to copy data to epicsInt32 buffer for callback */
                pasynManager->getAddr(pasynUser, &offset);
                dataType = getDataType(pasynUser);
                for (i=0; i<modbusLength_ && offset < modbusLength_; i++) {
                    readPlcInt(dataType, offset, &int32Data[i], &bufferLen);
                    offset += bufferLen;
                }
                /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
                pasynUser->auxStatus = ioStatus_;
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                          "%s::%s, calling client %p"
                          "callback=%p\n",
                           driverName, functionName, pInt32Array, pInt32Array->callback);
                pInt32Array->callback(pInt32Array->userPvt, pasynUser,
                                      int32Data, i);
                pnode = (interruptNode *)ellNext(&pnode->node);
            }
            pasynManager->interruptEnd(asynStdInterfaces.int32ArrayInterruptPvt);
        }

        /* See if there are any asynOctet callbacks registered to be called
         * when data changes.  These callbacks only happen if any data in this port has changed */
        if (forceCallback_ || anyChanged){
            pasynManager->interruptStart(asynStdInterfaces.octetInterruptPvt, &pclientList);
            pnode = (interruptNode *)ellFirst(pclientList);
            while (pnode) {
                asynOctetInterrupt *pOctet;
                pOctet = (asynOctetInterrupt *)pnode->drvPvt;
                pasynUser = pOctet->pasynUser;
                if (pasynUser->reason != P_Data) {
                    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                              "%s::%s port %s invalid pasynUser->reason %d\n",
                              driverName, functionName, this->portName, pasynUser->reason);
                    break;
                }
                pasynManager->getAddr(pasynUser, &offset);
                dataType = getDataType(pasynUser);
                if (checkOffset(offset)) {
                    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                              "%s::%s port %s invalid offset %d, max=%d\n",
                              driverName, functionName, this->portName, offset, modbusLength_);
                    break;
                }
                readPlcString(dataType, offset, stringBuffer, sizeof(stringBuffer), &bufferLen);
                /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
                pasynUser->auxStatus = ioStatus_;
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                          "%s::%s, calling client %p"
                          " callback=%p, data=%s\n",
                          driverName, functionName, pOctet, pOctet->callback, stringBuffer);
                pOctet->callback(pOctet->userPvt, pasynUser, stringBuffer, bufferLen, ASYN_EOM_CNT);
                pnode = (interruptNode *)ellNext(&pnode->node);
            }
            pasynManager->interruptEnd(asynStdInterfaces.octetInterruptPvt);
        }
                
        /* Reset the forceCallback flag */
        forceCallback_ = false;

        /* Set the previous I/O status */
        prevIOStatus = ioStatus_;

        /* Copy the new data to the previous data */
        memcpy(prevData, data_, modbusLength_*sizeof(epicsUInt16));
    }
}


asynStatus drvModbusAsyn::doModbusIO(int slave, int function, int start, 
                                     epicsUInt16 *data, int len)
{
    modbusReadRequest *readReq;
    modbusReadResponse *readResp;
    modbusWriteSingleRequest *writeSingleReq;
    /* modbusWriteSingleResponse *writeSingleResp; */
    modbusWriteMultipleRequest *writeMultipleReq;
    /* modbusWriteMultipleResponse *writeMultipleResp; */
    modbusReadWriteMultipleRequest *readWriteMultipleReq;
    modbusExceptionResponse *exceptionResp;
    int requestSize=0;
    int replySize;
    unsigned char  *pCharIn, *pCharOut;
    epicsUInt16 *pShortIn, *pShortOut;
    epicsUInt16 bitOutput;
    int byteCount;
    asynStatus status=asynSuccess;
    int i;
    epicsTimeStamp startTime, endTime;
    size_t nwrite, nread;
    int eomReason;
    double dT;
    int msec;
    int bin;
    unsigned char mask=0;
    int autoConnect;
    static const char *functionName = "doModbusIO";
 
    /* If the Octet driver is not set for autoConnect then do connection management ourselves */
    status = pasynManager->isAutoConnect(pasynUserOctet_, &autoConnect);
    if (!autoConnect) {
        /* See if we are connected */
        int itemp;
        status = pasynManager->isConnected(pasynUserOctet_, &itemp);
        isConnected_ = (itemp != 0) ? true : false;
         /* If we have an I/O error or are disconnected then disconnect device and reconnect */
        if ((ioStatus_ != asynSuccess) || !isConnected_) {
            if (ioStatus_ != asynSuccess) 
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                          "%s::%s port %s has I/O error\n",
                          driverName, functionName, this->portName);
            if (!isConnected_) 
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                          "%s::%s port %s is disconnected\n",
                          driverName, functionName, this->portName);
            status = pasynCommonSyncIO->disconnectDevice(pasynUserCommon_);
            if (status == asynSuccess) {
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                          "%s::%s port %s disconnect device OK\n",
                          driverName, functionName, this->portName);
            } else {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                          "%s::%s port %s disconnect error=%s\n",
                          driverName, functionName, this->portName, pasynUserOctet_->errorMessage);
            }
            status = pasynCommonSyncIO->connectDevice(pasynUserCommon_);
            if (status == asynSuccess) {
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                          "%s::%s port %s connect device OK\n",
                          driverName, functionName, this->portName);
            } else {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                          "%s::%s port %s connect device error=%s\n",
                          driverName, functionName, this->portName, pasynUserOctet_->errorMessage);
                goto done;
            }
        }
    }
        
    switch (function) {
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
            readReq = (modbusReadRequest *)modbusRequest_;
            readReq->slave = slave;
            readReq->fcode = function;
            readReq->startReg = htons((epicsUInt16)start);
            readReq->numRead = htons((epicsUInt16)len);
            requestSize = sizeof(modbusReadRequest);
            /* The -1 below is because the modbusReadResponse struct already has 1 byte of data */
            replySize = sizeof(modbusReadResponse) - 1 + len/8;
            if (len % 8) replySize++;
            break;
        case MODBUS_READ_HOLDING_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS:
            readReq = (modbusReadRequest *)modbusRequest_;
            readReq->slave = slave;
            readReq->fcode = function;
            readReq->startReg = htons((epicsUInt16)start);
            readReq->numRead = htons((epicsUInt16)len);
            requestSize = sizeof(modbusReadRequest);
            /* The -1 below is because the modbusReadResponse struct already has 1 byte of data */
            replySize = sizeof(modbusReadResponse) - 1 + len*2;
            break;
        case MODBUS_READ_INPUT_REGISTERS_F23:
            readWriteMultipleReq = (modbusReadWriteMultipleRequest *)modbusRequest_;
            readWriteMultipleReq->slave = slave;
            readWriteMultipleReq->fcode = MODBUS_READ_WRITE_MULTIPLE_REGISTERS;
            readWriteMultipleReq->startReadReg = htons((epicsUInt16)start);
            readWriteMultipleReq->numRead = htons((epicsUInt16)len);
            readWriteMultipleReq->startWriteReg = htons((epicsUInt16)start);
            /* It seems that one cannot specify numOutput=0 to not write values, at least the Modbus Slave
             * simulator does not allow this.  
             * But nothing will be written if the data part of the message is 0 length.
             * So we set numOutput to one word and set the message so the data length is 0. */
            readWriteMultipleReq->numOutput = htons(1);
            readWriteMultipleReq->byteCount = 2;
            /* The -1 below is because the modbusReadWriteMultipleRequest struct already has 1 byte of data,
               and we don't want to send it. */
            requestSize = sizeof(modbusReadWriteMultipleRequest) - 1;
            /* The -1 below is because the modbusReadResponse struct already has 1 byte of data */
            replySize = sizeof(modbusReadResponse) - 1 + len*2;
            break;
        case MODBUS_WRITE_SINGLE_COIL:
            writeSingleReq = (modbusWriteSingleRequest *)modbusRequest_;
            writeSingleReq->slave = slave;
            writeSingleReq->fcode = function;
            writeSingleReq->startReg = htons((epicsUInt16)start);
            if (*data) bitOutput = 0xFF00;
            else       bitOutput = 0;
            writeSingleReq->data = htons(bitOutput);
            requestSize = sizeof(modbusWriteSingleRequest);
            replySize = sizeof(modbusWriteSingleResponse);
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                      "%s::%s port %s WRITE_SINGLE_COIL"
                      " address=0%o value=0x%x\n",
                      driverName, functionName, this->portName, start, bitOutput);
            break;
        case MODBUS_WRITE_SINGLE_REGISTER:
            writeSingleReq = (modbusWriteSingleRequest *)modbusRequest_;
            writeSingleReq->slave = slave;
            writeSingleReq->fcode = function;
            writeSingleReq->startReg = htons((epicsUInt16)start);
            writeSingleReq->data = (epicsUInt16)*data;
            writeSingleReq->data = htons(writeSingleReq->data);
            requestSize = sizeof(modbusWriteSingleRequest);
            replySize = sizeof(modbusWriteSingleResponse);
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                      "%s::%s port %s WRITE_SINGLE_REGISTER"
                      " address=0%o value=0x%x\n",
                      driverName, functionName, this->portName, start, *data);
            break;
        case MODBUS_WRITE_MULTIPLE_COILS:
            writeMultipleReq = (modbusWriteMultipleRequest *)modbusRequest_;
            writeMultipleReq->slave = slave;
            writeMultipleReq->fcode = function;
            writeMultipleReq->startReg = htons((epicsUInt16)start);
            /* Pack bits into output */
            pShortIn = (epicsUInt16 *)data;
            pCharOut = (unsigned char *)&writeMultipleReq->data;
            /* Subtract 1 because it will be incremented first time */
            pCharOut--;
            for (i=0; i<len; i++) {
                if (i%8 == 0) {
                    mask = 0x01;
                    pCharOut++;
                    *pCharOut = 0;
                }
                *pCharOut |= ((*pShortIn++ ? 0xFF:0) & mask);
                mask = mask << 1;
            }
            writeMultipleReq->numOutput = htons(len);
            byteCount = (int) (pCharOut - writeMultipleReq->data + 1);
            writeMultipleReq->byteCount = byteCount;
            asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                        (char *)writeMultipleReq->data, byteCount, 
                        "%s::%s port %s WRITE_MULTIPLE_COILS\n",
                        driverName, functionName, this->portName);
            requestSize = sizeof(modbusWriteMultipleRequest) + byteCount - 1;
            replySize = sizeof(modbusWriteMultipleResponse);
            break;
        case MODBUS_WRITE_MULTIPLE_REGISTERS:
            writeMultipleReq = (modbusWriteMultipleRequest *)modbusRequest_;
            writeMultipleReq->slave = slave;
            writeMultipleReq->fcode = function;
            writeMultipleReq->startReg = htons((epicsUInt16)start);
            pShortIn = (epicsUInt16 *)data;
            pShortOut = (epicsUInt16 *)&writeMultipleReq->data;
            for (i=0; i<len; i++, pShortOut++) {
                *pShortOut = htons(*pShortIn++);
            }
            writeMultipleReq->numOutput = htons(len);
            byteCount = 2*len;
            writeMultipleReq->byteCount = byteCount;
            asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                        (char *)writeMultipleReq->data, byteCount, 
                        "%s::%s port %s WRITE_MULTIPLE_REGISTERS\n",
                        driverName, functionName, this->portName);
            requestSize = sizeof(modbusWriteMultipleRequest) + byteCount - 1;
            replySize = sizeof(modbusWriteMultipleResponse);
            break;
        case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
            readWriteMultipleReq = (modbusReadWriteMultipleRequest *)modbusRequest_;
            readWriteMultipleReq->slave = slave;
            readWriteMultipleReq->fcode = MODBUS_READ_WRITE_MULTIPLE_REGISTERS;
            readWriteMultipleReq->startReadReg = htons((epicsUInt16)start);
            /* We don't actually do anything with the values read from the device, but it does not
             * seem to be allowed to specify numRead=0, so we always read one word from the same address
             * we write to. */
            nread = 1;
            readWriteMultipleReq->numRead = htons((epicsUInt16)nread);
            readWriteMultipleReq->startWriteReg = htons((epicsUInt16)start);
            pShortIn = (epicsUInt16 *)data;
            pShortOut = (epicsUInt16 *)&readWriteMultipleReq->data;
            for (i=0; i<len; i++, pShortOut++) {
                *pShortOut = htons(*pShortIn++);
            }
            readWriteMultipleReq->numOutput = htons(len);
            byteCount = 2*len;
            readWriteMultipleReq->byteCount = byteCount;
            asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                        (char *)readWriteMultipleReq->data, byteCount, 
                        "%s::%s port %s WRITE_MULTIPLE_REGISTERS_F23\n",
                        driverName, functionName, this->portName);
            requestSize = sizeof(modbusReadWriteMultipleRequest) + byteCount - 1;
            /* The -1 below is because the modbusReadResponse struct already has 1 byte of data */
            replySize = (int)(sizeof(modbusReadResponse) + 2*nread - 1);
            break;


        default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                      "%s::%s, port %s unsupported function code %d\n", 
                      driverName, functionName, this->portName, function);
            status = asynError;
            goto done;
    }

    /* Do the Modbus I/O as a write/read cycle */
    epicsTimeGetCurrent(&startTime);
    status = pasynOctetSyncIO->writeRead(pasynUserOctet_, 
                                         modbusRequest_, requestSize,
                                         modbusReply_, replySize,
                                         MODBUS_READ_TIMEOUT,
                                         &nwrite, &nread, &eomReason);
    epicsTimeGetCurrent(&endTime);

    if (status != prevIOStatus_) {                                      
      if (status != asynSuccess) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                     "%s::%s port %s error calling writeRead,"
                     " error=%s, nwrite=%d/%d, nread=%d\n", 
                     driverName, functionName, this->portName, 
                     pasynUserOctet_->errorMessage, (int)nwrite, requestSize, (int)nread);
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                     "%s::%s port %s writeRead status back to normal having had %d errors,"
                     " nwrite=%d/%d, nread=%d\n", 
                     driverName, functionName, this->portName, 
                     currentIOErrors_, (int)nwrite, requestSize, (int)nread);
            currentIOErrors_ = 0;
        }
        prevIOStatus_ = status;
    }
    if (status != asynSuccess) {
        IOErrors_++;
        setIntegerParam(P_IOErrors, IOErrors_);
        currentIOErrors_++;
        goto done;
    }

    dT = epicsTimeDiffInSeconds(&endTime, &startTime);
    msec = (int)(dT*1000. + 0.5);
    lastIOMsec_ = msec;
    setIntegerParam(P_LastIOTime, msec);
    if (msec > maxIOMsec_) {
      maxIOMsec_ = msec;
      setIntegerParam(P_MaxIOTime, msec);
    }
    if (enableHistogram_) {
        bin = msec /histogramMsPerBin_;
        if (bin < 0) bin = 0;
        /* Longer times go in last bin of histogram */
        if (bin >= HISTOGRAM_LENGTH-1) bin = HISTOGRAM_LENGTH-1; 
        timeHistogram_[bin]++;
    }     

    /* See if there is a Modbus exception */
    readResp = (modbusReadResponse *)modbusReply_;
    if (readResp->fcode & MODBUS_EXCEPTION_FCN) {
        exceptionResp = (modbusExceptionResponse *)modbusReply_;
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s port %s Modbus exception=%d\n", 
                  driverName, functionName, this->portName, exceptionResp->exception);
        status = asynError;
        goto done;
    }

    /* Make sure the function code in the response is the same as the one 
     * in the request? */

    switch (function) {
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
            readOK_++;
            setIntegerParam(P_ReadOK, readOK_);
            readResp = (modbusReadResponse *)modbusReply_;
            nread = readResp->byteCount;
            pCharIn = (unsigned char *)&readResp->data;
            /* Subtract 1 because it will be incremented first time */
            pCharIn--;
            /* We assume we got len bits back, since we are only told bytes */
            for (i=0; i<len; i++) {
                if (i%8 == 0) {
                    mask = 0x01;
                    pCharIn++;
                }
                data[i] = (*pCharIn & mask) ? 1:0;
                mask = mask << 1;
            }
            asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                        (char *)data, len*2, 
                        "%s::%s port %s READ_COILS\n",
                        driverName, functionName, this->portName);
            break;
        case MODBUS_READ_HOLDING_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS_F23:
            readOK_++;
            setIntegerParam(P_ReadOK, readOK_);
            readResp = (modbusReadResponse *)modbusReply_;
            nread = readResp->byteCount/2;
            pShortIn = (epicsUInt16 *)&readResp->data;
            /* Check to make sure we got back the expected number of words */
            if ((int)nread != len) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s, port %s expected %d words, actually received %d\n", 
                          driverName, functionName, this->portName, len, (int)nread);
                status = asynError;
                goto done;
            }
            for (i=0; i<(int)nread; i++) { 
                data[i] = ntohs(pShortIn[i]);
            }
            asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                        (char *)data, nread*2, 
                        "%s::%s port %s READ_REGISTERS\n",
                        driverName, functionName, this->portName);
            break;

        /* We don't do anything with responses to writes for now.  
         * Could add error checking. */
        case MODBUS_WRITE_SINGLE_COIL:
        case MODBUS_WRITE_SINGLE_REGISTER:
            writeOK_++;
            setIntegerParam(P_WriteOK, writeOK_);
            /* Not using value for now so comment out to avoid compiler warning
            writeSingleResp = (modbusWriteSingleResponse *)modbusReply_;
            */
            break;
        case MODBUS_WRITE_MULTIPLE_COILS:
        case MODBUS_WRITE_MULTIPLE_REGISTERS:
            writeOK_++;
            setIntegerParam(P_WriteOK, writeOK_);
            /* Not using value for now so comment out to avoid compiler warning
            writeMultipleResp = (modbusWriteMultipleResponse *)modbusReply_;
            */
            break;
        case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
            writeOK_++;
            setIntegerParam(P_WriteOK, writeOK_);
            //readResp = (modbusReadResponse *)modbusReply_;
            break;
        default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s, port %s unsupported function code %d\n", 
                      driverName, functionName, this->portName, function);
            status = asynError;
            goto done;
    }

    done:
    return status;
}

modbusDataType_t drvModbusAsyn::getDataType(asynUser *pasynUser)
{
    modbusDataType_t dataType;
    if (pasynUser->drvUser) {
        dataType = *(modbusDataType_t *)pasynUser->drvUser;
    } else {
        dataType = dataType_;
    }
    return dataType;
}

asynStatus drvModbusAsyn::checkOffset(int offset)
{
    if (offset < 0) return asynError;
    if (absoluteAddressing_) {
        if (offset > 65535) return asynError;
    } else {
        if (offset >= modbusLength_) return asynError;
    }
    return asynSuccess;
}

asynStatus drvModbusAsyn::checkModbusFunction(int *modbusFunction)
{
    /* If this is an initial read operation on a write function code and 
     * pollDelay is not > 0 then return error */
    if (readOnceFunction_ && (pollDelay_ <= 0)) return asynError;
    *modbusFunction = readOnceFunction_ ? readOnceFunction_ : modbusFunction_;
    return asynSuccess;
}



asynStatus drvModbusAsyn::readPlcInt(modbusDataType_t dataType, int offset, epicsInt32 *output, int *bufferLen)
{
    epicsUInt16 value;
    epicsInt32 result=0;
    asynStatus status = asynSuccess;
    epicsFloat64 fValue;
    int i;
    int mult=1;
    int signMask = 0x8000;
    int negative = 0;
    int littleWord = (EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE) ? 0 : 1; 
    int bigWord    = (EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE) ? 1 : 0; 
    union {
        epicsInt32 int32;
        epicsUInt16 uint16[2];
    } int16_32;
    static const char *functionName="readPlcInt";
    
    value = data_[offset];
    *bufferLen = 1;
    switch (dataType) {
        case dataTypeUInt16:
            result = value;
            break;

        case dataTypeInt16SM:
            result = value;
            if (result & signMask) {
                result &= ~signMask;
                result = -(epicsInt16)result;
            }
            break;

        case dataTypeBCDSigned:
            if (value & signMask) {
                negative=1;
                value &= ~signMask;
            } /* Note: no break here! */
        case dataTypeBCDUnsigned:
            for (i=0; i<4; i++) {
                result += (value & 0xF)*mult;
                mult = mult*10;
                value = value >> 4;
            }
            if (negative) result = -result;
            break;

        case dataTypeInt16:
            result = (epicsInt16)value;
            break;

        case dataTypeInt32LE:
            int16_32.uint16[littleWord] = data_[offset];
            int16_32.uint16[bigWord]    = data_[offset+1];
            result = int16_32.int32;
            *bufferLen = 2;
            break;

        case dataTypeInt32BE:
            int16_32.uint16[bigWord]    = data_[offset];
            int16_32.uint16[littleWord] = data_[offset+1];
            result = int16_32.int32;
            *bufferLen = 2;
            break;

        case dataTypeFloat32LE:
        case dataTypeFloat32BE:        
        case dataTypeFloat64LE:
        case dataTypeFloat64BE:        
            status = readPlcFloat(dataType, offset, &fValue, bufferLen);
            result = (epicsInt32)fValue;
            break;

        default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s, port %s unknown data type %d\n", 
                      driverName, functionName, this->portName, dataType);
            status = asynError;
    }
    *output = result;
    return status;
}


asynStatus drvModbusAsyn::writePlcInt(modbusDataType_t dataType, int offset, epicsInt32 value, epicsUInt16 *buffer, int *bufferLen)
{
    epicsUInt16 result=0;
    int i;
    int signMask = 0x8000;
    int div=1000;
    int digit;
    int negative = 0;
    int littleWord = (EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE) ? 0 : 1; 
    int bigWord    = (EPICS_BYTE_ORDER == EPICS_ENDIAN_LITTLE) ? 1 : 0; 
    asynStatus status = asynSuccess;
    union {
        epicsInt32 int32;
        epicsUInt16 uint16[2];
    } int16_32;
    static const char *functionName="writePlcInt";

    *bufferLen = 1;
    switch (dataType) {
        case dataTypeUInt16:
            buffer[0] = (epicsUInt16)value;
            break;
 
        case dataTypeInt16SM:
            result = value;
            if (result & signMask) {
                result = -(short)result;
                result |= signMask;
            }
            buffer[0] = result;
            break;

        case dataTypeBCDSigned:
            if ((short)value < 0) {
                negative=1;
                value = -(short)value;
            } /* Note: no break here */
        case dataTypeBCDUnsigned:
            for (i=0; i<4; i++) {
                result = result << 4;
                digit = value / div;
                result |= digit;
                value = value - digit*div;
                div = div/10;
            }
            if (negative) result |= signMask;
            buffer[0] = result;
            break;

        case dataTypeInt16:
            buffer[0] = (epicsInt16)value;
            break;
            
        case dataTypeInt32LE:
            int16_32.int32 = value;
            buffer[0] = int16_32.uint16[littleWord];
            buffer[1] = int16_32.uint16[bigWord];
            *bufferLen = 2;
            break;
            
        case dataTypeInt32BE:
            int16_32.int32 = value;
            buffer[0] = int16_32.uint16[bigWord];
            buffer[1] = int16_32.uint16[littleWord];
            *bufferLen = 2;
            break;
            
        case dataTypeFloat32LE:
        case dataTypeFloat32BE:        
        case dataTypeFloat64LE:
        case dataTypeFloat64BE:        
            status = writePlcFloat(dataType, offset, (epicsFloat64)value, buffer, bufferLen);
            break;

        default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s, port %s unknown data type %d\n", 
                      driverName, functionName, this->portName, dataType);
            status = asynError;
    }

    return status;
}

asynStatus drvModbusAsyn::readPlcFloat(modbusDataType_t dataType, int offset, epicsFloat64 *output, int *bufferLen)
{
    union {
        epicsFloat32 f32;
        epicsFloat64 f64;
        epicsUInt16  ui16[4];
    } uIntFloat;
    epicsInt32 iValue;
    asynStatus status = asynSuccess;
    /* Default to little-endian */
    int w32_0=0, w32_1=1, w64_0=0, w64_1=1, w64_2=2, w64_3=3;
    if (EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_BIG){
        w32_0=1; w32_1=0; w64_0=3; w64_1=2; w64_2=1; w64_3=0;
    }
    static const char *functionName="readPlcFloat";
    
    switch (dataType) {
        case dataTypeUInt16:
        case dataTypeInt16SM:
        case dataTypeBCDSigned:
        case dataTypeBCDUnsigned:
        case dataTypeInt16:
        case dataTypeInt32LE:
        case dataTypeInt32BE:
            status = readPlcInt(dataType, offset, &iValue, bufferLen);
            *output = (epicsFloat64)iValue;
            break;
            
        case dataTypeFloat32LE:
            uIntFloat.ui16[w32_0] = data_[offset];
            uIntFloat.ui16[w32_1] = data_[offset+1];
            *output = (epicsFloat64)uIntFloat.f32;
            *bufferLen = 2;
            break;
            
        case dataTypeFloat32BE:
            uIntFloat.ui16[w32_1] = data_[offset];
            uIntFloat.ui16[w32_0] = data_[offset+1];
            *output = (epicsFloat64)uIntFloat.f32;
            *bufferLen = 2;
            break;
            
        case dataTypeFloat64LE:
            uIntFloat.ui16[w64_0] = data_[offset];
            uIntFloat.ui16[w64_1] = data_[offset+1];
            uIntFloat.ui16[w64_2] = data_[offset+2];
            uIntFloat.ui16[w64_3] = data_[offset+3];
            *output = (epicsFloat64)uIntFloat.f64;
            *bufferLen = 4;
            break;
            
        case dataTypeFloat64BE:
            uIntFloat.ui16[w64_3] = data_[offset];
            uIntFloat.ui16[w64_2] = data_[offset+1];
            uIntFloat.ui16[w64_1] = data_[offset+2];
            uIntFloat.ui16[w64_0] = data_[offset+3];
            *output = (epicsFloat64)uIntFloat.f64;
            *bufferLen = 4;
            break;
            
        default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s, port %s unknown data type %d\n", 
                      driverName, functionName, this->portName, dataType);
            status = asynError;
    }
    return status;
}


asynStatus drvModbusAsyn::writePlcFloat(modbusDataType_t dataType, int offset, epicsFloat64 value, epicsUInt16 *buffer, int *bufferLen)
{
    union {
        epicsFloat32 f32;
        epicsFloat64 f64;
        epicsUInt16  ui16[4];
    } uIntFloat;
    asynStatus status = asynSuccess;
    /* Default to little-endian */
    int w32_0=0, w32_1=1, w64_0=0, w64_1=1, w64_2=2, w64_3=3;
    if (EPICS_FLOAT_WORD_ORDER == EPICS_ENDIAN_BIG){
        w32_0=1; w32_1=0; w64_0=3; w64_1=2; w64_2=1; w64_3=0;
    }
    static const char *functionName="writePlcFloat";
    
    switch (dataType) {
        case dataTypeUInt16:
        case dataTypeInt16SM:
        case dataTypeBCDSigned:
        case dataTypeBCDUnsigned:
        case dataTypeInt16:
        case dataTypeInt32LE:
        case dataTypeInt32BE:
            status = writePlcInt(dataType, offset, (epicsInt32)value, buffer, bufferLen);
            break;
            
        case dataTypeFloat32LE:
            *bufferLen = 2;
            uIntFloat.f32 = (epicsFloat32)value;
            buffer[0] = uIntFloat.ui16[w32_0];
            buffer[1] = uIntFloat.ui16[w32_1];
            break;
            
        case dataTypeFloat32BE:
            *bufferLen = 2;
            uIntFloat.f32 = (epicsFloat32)value;
            buffer[0] = uIntFloat.ui16[w32_1];
            buffer[1] = uIntFloat.ui16[w32_0];
            break;

        case dataTypeFloat64LE:
            *bufferLen = 4;
            uIntFloat.f64 = value;
            buffer[0] = uIntFloat.ui16[w64_0];
            buffer[1] = uIntFloat.ui16[w64_1];
            buffer[2] = uIntFloat.ui16[w64_2];
            buffer[3] = uIntFloat.ui16[w64_3];
            break;
            
        case dataTypeFloat64BE:
            *bufferLen = 4;
            uIntFloat.f64 = value;
            buffer[0] = uIntFloat.ui16[w64_3];
            buffer[1] = uIntFloat.ui16[w64_2];
            buffer[2] = uIntFloat.ui16[w64_1];
            buffer[3] = uIntFloat.ui16[w64_0];
            break;
            
        default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s, port %s unsupported data type %d\n", 
                      driverName, functionName, this->portName, dataType);
            status = asynError;
    }
    return status;
}

asynStatus drvModbusAsyn::readPlcString(modbusDataType_t dataType, int offset, 
                                        char *data, size_t maxChars, int *bufferLen)
{
    size_t i;
    asynStatus status = asynSuccess;
    static const char *functionName="readPlcString";

    for (i=0; i<maxChars && offset<modbusLength_; i++, offset++) {
        switch (dataType) {
            case dataTypeStringHigh:
                data[i] = (data_[offset] >> 8) & 0x00ff;
                break;

            case dataTypeStringLow:
                data[i] = data_[offset] & 0x00ff;
                break;

            case dataTypeStringHighLow:
                data[i] = (data_[offset] >> 8) & 0x00ff;
                if (i<maxChars-1) {
                    i++;
                    data[i] = data_[offset] & 0x00ff;
                }
                break;

            case dataTypeStringLowHigh:
                data[i] = data_[offset] & 0x00ff;
                if (i<maxChars-1) {
                    i++;
                    data[i] = (data_[offset] >> 8) & 0x00ff;
                }
                break;

            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s, port %s unknown data type %d\n", 
                          driverName, functionName, this->portName, dataType);
                status = asynError;
        }
    }
    /* Nil terminate and set number of characters to include trailing nil */
    if (i >= maxChars) {
        i = maxChars-1;
    } 
    data[i] = 0;
    *bufferLen = (int)(strlen(data) + 1);
    return status;
}

asynStatus drvModbusAsyn::writePlcString(modbusDataType_t dataType, int offset, 
                                        const char *data, size_t maxChars, size_t *nActual, int *bufferLen)
{
    size_t i;
    asynStatus status = asynSuccess;
    static const char *functionName="writePlcString";

    for (i=0, *bufferLen=0, *nActual=0; i<maxChars && offset<modbusLength_; i++, offset++) {
        switch (dataType) {
            case dataTypeStringHigh:
                data_[offset] = (data[i] << 8) & 0xff00;
                break;

            case dataTypeStringLow:
                data_[offset] = data[i] & 0x00ff;
                break;

            case dataTypeStringHighLow:
                data_[offset] = (data[i] << 8) & 0xff00;
                if (i<maxChars-1) {
                    i++;
                    data_[offset] |= data[i] & 0x00ff;
                }
                break;

            case dataTypeStringLowHigh:
                data_[offset] = data[i] & 0x00ff;
                if (i<maxChars-1) {
                    i++;
                    data_[offset] |= (data[i] << 8) & 0xff00;
                }
                break;

            default:
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                          "%s::%s, port %s unknown data type %d\n", 
                          driverName, functionName, this->portName, dataType);
                status = asynError;
        }
        *nActual = i + 1;
        (*bufferLen)++;
    }
    return status;
}

extern "C" {
/*
** drvModbusAsynConfigure() - create and init an asyn port driver for a PLC
**                                                                    
*/

/** EPICS iocsh callable function to call constructor for the drvModbusAsyn class. */
asynStatus drvModbusAsynConfigure(const char *portName, const char *octetPortName, 
                                  int modbusSlave, int modbusFunction, 
                                  int modbusStartAddress, int modbusLength,
                                  int dataType,
                                  int pollMsec, 
                                  char *plcType)
{
    new drvModbusAsyn(portName, octetPortName, 
                      modbusSlave, modbusFunction, 
                      modbusStartAddress, modbusLength,
                      (modbusDataType_t)dataType,
                      pollMsec, 
                      plcType);
    return(asynSuccess);
}

/* iocsh functions */

static const iocshArg ConfigureArg0 = {"Port name",            iocshArgString};
static const iocshArg ConfigureArg1 = {"Octet port name",      iocshArgString};
static const iocshArg ConfigureArg2 = {"Modbus slave address", iocshArgInt};
static const iocshArg ConfigureArg3 = {"Modbus function code", iocshArgInt};
static const iocshArg ConfigureArg4 = {"Modbus start address", iocshArgInt};
static const iocshArg ConfigureArg5 = {"Modbus length",        iocshArgInt};
static const iocshArg ConfigureArg6 = {"Data type",            iocshArgInt};
static const iocshArg ConfigureArg7 = {"Poll time (msec)",     iocshArgInt};
static const iocshArg ConfigureArg8 = {"PLC type",             iocshArgString};

static const iocshArg * const drvModbusAsynConfigureArgs[9] = {
    &ConfigureArg0,
    &ConfigureArg1,
    &ConfigureArg2,
    &ConfigureArg3,
    &ConfigureArg4,
    &ConfigureArg5,
    &ConfigureArg6,
    &ConfigureArg7,
    &ConfigureArg8
};

static const iocshFuncDef drvModbusAsynConfigureFuncDef=
                                                    {"drvModbusAsynConfigure", 9,
                                                     drvModbusAsynConfigureArgs};
static void drvModbusAsynConfigureCallFunc(const iocshArgBuf *args)
{
  drvModbusAsynConfigure(args[0].sval, args[1].sval, args[2].ival, args[3].ival, args[4].ival, 
                         args[5].ival, args[6].ival, args[7].ival, args[8].sval);
}


static void drvModbusAsynRegister(void)
{
  iocshRegister(&drvModbusAsynConfigureFuncDef,drvModbusAsynConfigureCallFunc);
}

epicsExportRegistrar(drvModbusAsynRegister);

} // extern "C"