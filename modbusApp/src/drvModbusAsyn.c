/*----------------------------------------------------------------------
 *  file:        drvModbusAsyn.c                                         
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
#include "asynDriver.h"
#include "asynOctetSyncIO.h"
#include "asynCommonSyncIO.h"
#include "asynStandardInterfaces.h"

#include "modbus.h"
#include "drvModbusAsyn.h"
#include <epicsExport.h>

/* Defined constants */

#define MAX_READ_WORDS       125        /* Modbus limit on number of words to read */
#define MAX_WRITE_WORDS      123        /* Modbus limit on number of words to write */
#define HISTOGRAM_LENGTH     200        /* Length of time histogram */
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


/* Structures for drvUser interface */

typedef enum {
    modbusDataCommand,
    modbusReadCommand,
    modbusEnableHistogramCommand,
    modbusReadHistogramCommand,
    modbusHistogramBinTimeCommand,
    modbusHistogramTimeAxisCommand,
    modbusPollDelayCommand,
    modbusReadOKCommand,
    modbusWriteOKCommand,
    modbusIOErrorsCommand,
    modbusLastIOTimeCommand,
    modbusMaxIOTimeCommand
} modbusCommand;

/* Note, this constant must match the number of enums in modbusCommand */
#define MAX_MODBUS_COMMANDS 12

typedef struct {
    modbusCommand command;
    char *commandString;
} modbusCommandStruct;

static modbusCommandStruct modbusCommands[MAX_MODBUS_COMMANDS] = {
    {modbusDataCommand,            MODBUS_DATA_STRING},    
    {modbusReadCommand,            MODBUS_READ_STRING},    
    {modbusEnableHistogramCommand, MODBUS_ENABLE_HISTOGRAM_STRING},
    {modbusReadHistogramCommand,   MODBUS_READ_HISTOGRAM_STRING}, 
    {modbusHistogramBinTimeCommand,  MODBUS_HISTOGRAM_BIN_TIME_STRING}, 
    {modbusHistogramTimeAxisCommand, MODBUS_HISTOGRAM_TIME_AXIS_STRING}, 
    {modbusPollDelayCommand,       MODBUS_POLL_DELAY_STRING}, 
    {modbusReadOKCommand,          MODBUS_READ_OK_STRING}, 
    {modbusWriteOKCommand,         MODBUS_WRITE_OK_STRING}, 
    {modbusIOErrorsCommand,        MODBUS_IO_ERRORS_STRING},
    {modbusLastIOTimeCommand,      MODBUS_LAST_IO_TIME_STRING},
    {modbusMaxIOTimeCommand,       MODBUS_MAX_IO_TIME_STRING} 
};

typedef struct {
    modbusDataType_t dataType;
    char *dataTypeString;
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



/* The structure for the drvModbus asyn port or "object" */

typedef struct modbusStr *PLC_ID;

typedef struct modbusStr
{
    char *portName;             /* asyn port name for this server */
    char *octetPortName;        /* asyn port name for the asyn octet port */
    char *plcType;              /* String describing PLC type */
    int isConnected;            /* Connection status */
    int ioStatus;               /* I/O error status */
    asynUser  *pasynUserOctet;  /* asynUser for asynOctet interface to asyn octet port */ 
    asynUser  *pasynUserCommon; /* asynUser for asynCommon interface to asyn octet port */
    asynUser  *pasynUserTrace;  /* asynUser for asynTrace on this port */
    asynStandardInterfaces asynStdInterfaces;  /* Structure for standard interfaces */
    int modbusSlave;            /* Modbus slave address */
    int modbusFunction;         /* Modbus function code */
    int modbusStartAddress;     /* Modbus starting addess for this port */
    int modbusLength;           /* Number of words or bits of Modbus data */
    int absoluteAddressing;     /* Address from asyn are absolute, rather than relative to modbusStartAddress */
    modbusDataType_t dataType;  /* Data type */
    epicsUInt16 *data;          /* Memory buffer */
    char modbusRequest[MAX_MODBUS_FRAME_SIZE];      /* Modbus request message */
    char modbusReply[MAX_MODBUS_FRAME_SIZE];        /* Modbus reply message */
    double pollDelay;           /* Delay for readPoller */
    epicsThreadId readPollerThreadId;
    epicsEventId readPollerEventId;
    int forceCallback;
    int readOnceFunction;
    int readOnceDone;
    asynStatus prevIOStatus;
    int readOK;                 /* Statistics */
    int writeOK;
    int IOErrors;
    int maxIOMsec;
    int lastIOMsec; 
    epicsInt32 timeHistogram[HISTOGRAM_LENGTH];     /* Histogram of read-times */
    epicsInt32 histogramTimeAxis[HISTOGRAM_LENGTH]; /* Time axis of histogram of read-times */
    int enableHistogram;
    int histogramMsPerBin;
    int readbackOffset;  /* Readback offset for Wago devices */
} modbusStr_t;


/* Local variable declarations */
static char *driver = "drvModbusAsyn";           /* String for asynPrint */
static int modbusInitialized = 0;
static int modbusExiting = 0;

/* Local function declarations */

/* These functions are in the asynCommon interface */
static void asynReport              (void *drvPvt, FILE *fp, int details);
static asynStatus asynConnect       (void *drvPvt, asynUser *pasynUser);
static asynStatus asynDisconnect    (void *drvPvt, asynUser *pasynUser);

static asynStatus drvUserCreate     (void *drvPvt, asynUser *pasynUser,
                                     const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserGetType    (void *drvPvt, asynUser *pasynUser,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserDestroy    (void *drvPvt, asynUser *pasynUser);

/* These functions are in the asynUInt32Digital interface */
static asynStatus writeUInt32D      (void *drvPvt, asynUser *pasynUser,
                                     epicsUInt32 value, epicsUInt32 mask);
static asynStatus readUInt32D       (void *drvPvt, asynUser *pasynUser,
                                     epicsUInt32 *value, epicsUInt32 mask);
static asynStatus setInterrupt      (void *drvPvt, asynUser *pasynUser,
                                     epicsUInt32 mask, interruptReason reason);
static asynStatus clearInterrupt    (void *drvPvt, asynUser *pasynUser,
                                     epicsUInt32 mask);
static asynStatus getInterrupt      (void *drvPvt, asynUser *pasynUser,
                                     epicsUInt32 *mask, interruptReason reason);

/* These functions are in the asynInt32 interface */
static asynStatus writeInt32        (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 value);
static asynStatus readInt32         (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *value);
static asynStatus getBounds         (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *low, epicsInt32 *high);

/* These functions are in the asynFloat64 interface */
static asynStatus writeFloat64      (void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 value);
static asynStatus readFloat64       (void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 *value);

/* These functions are in the asynInt32Array interface */
static asynStatus readInt32Array    (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *data, size_t maxChans,
                                     size_t *nactual);
static asynStatus writeInt32Array   (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *data, size_t maxChans);

/* These functions are in the asynOctet interface */
static asynStatus writeOctet        (void *drvPvt, asynUser *pasynUser,
                                     const char *value, size_t maxChars, 
                                     size_t *nActual);
static asynStatus readOctet         (void *drvPvt, asynUser *pasynUser,
                                     char *value, size_t maxChars, 
                                     size_t *nActual, int *eomReason);

/* These functions are not in any of the asyn interfaces */
static void readPoller(PLC_ID pPlc);
static modbusDataType_t getDataType(PLC_ID pPlc, asynUser *pasynUser);
static int checkOffset(PLC_ID pPlc, int offset);
static int checkModbusFunction(PLC_ID pPlc, int *modbusFunction);
static int doModbusIO(PLC_ID pPlc, int slave, int function, int start, 
                      epicsUInt16 *data, int len);
static asynStatus readPlcInt    (modbusStr_t *pPlc, modbusDataType_t dataType, int offset, 
                                 epicsInt32   *value,                      int *bufferLen);
static asynStatus writePlcInt   (modbusStr_t *pPlc, modbusDataType_t dataType, int offset, 
                                 epicsInt32    value, epicsUInt16 *buffer, int *bufferLen);
static asynStatus readPlcFloat  (modbusStr_t *pPlc, modbusDataType_t dataType, int offset, 
                                 epicsFloat64 *value,                      int *bufferLen);
static asynStatus writePlcFloat (modbusStr_t *pPlc, modbusDataType_t dataType, int offset, 
                                 epicsFloat64  value, epicsUInt16 *buffer, int *bufferLen);
static asynStatus readPlcString (modbusStr_t *pPlc, modbusDataType_t dataType, int offset, 
                                 char *value, size_t maxChars,             int *bufferLen);
static asynStatus writePlcString(modbusStr_t *pPlc, modbusDataType_t dataType, int offset, 
                                 const char *value, size_t maxChars, size_t *nActual, int *bufferLen);


/* asynCommon methods */
static const struct asynCommon drvCommon = {
    asynReport,
    asynConnect,
    asynDisconnect
};

/* asynDrvUser methods */
static asynDrvUser drvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};

/* asynUInt32Digital methods */
static struct asynUInt32Digital drvUInt32D = {
    writeUInt32D,
    readUInt32D,
    setInterrupt,
    clearInterrupt,
    getInterrupt,
    NULL,
    NULL
};

/* asynInt32 methods */
static asynInt32 drvInt32 = {
    writeInt32,
    readInt32,
    getBounds,
    NULL,
    NULL
};
/* asynFloat64 methods */
static asynFloat64 drvFloat64 = {
    writeFloat64,
    readFloat64,
    NULL,
    NULL
};

/* asynInt32Array methods */
static asynInt32Array drvInt32Array = {
    writeInt32Array,
    readInt32Array,
    NULL,
    NULL
};

static asynOctet drvOctet = {
    writeOctet,
    readOctet,
    NULL,  /*  flushOctet */
    NULL,  /* registerInterruptUser */
    NULL,  /* cancelInterruptUser */
    NULL,  /* setInputEosOctet */
    NULL,  /* getInputEosOctet */
    NULL,  /* setOutputEosOctet */
    NULL   /* getOutputEosOctet */
};



/********************************************************************
**  global driver functions
*********************************************************************
*/

static void modbusExitCallback(void *pPvt) {
    modbusExiting = 1;
}

/*
** drvModbusAsynConfigure() - create and init an asyn port driver for a PLC
**                                                                    
*/

int drvModbusAsynConfigure(char *portName, 
                           char *octetPortName, 
                           int modbusSlave,
                           int modbusFunction, 
                           int modbusStartAddress, 
                           int modbusLength,
                           modbusDataType_t dataType,
                           int pollMsec, 
                           char *plcType)
{
    int status;
    PLC_ID pPlc;
    char readThreadName[100];
    int needReadThread=0;
    int IOLength=0;
    int maxLength=0;

    if (plcType == NULL) plcType = "";
    pPlc = callocMustSucceed(1, sizeof(*pPlc), "drvModbusAsynConfigure");
    pPlc->portName = epicsStrDup(portName);
    pPlc->octetPortName = epicsStrDup(octetPortName);
    pPlc->plcType = epicsStrDup(plcType);
    pPlc->modbusSlave = modbusSlave;
    pPlc->modbusFunction = modbusFunction;
    pPlc->modbusStartAddress = modbusStartAddress;
    pPlc->dataType = dataType;
    if (modbusStartAddress == -1) {
        pPlc->absoluteAddressing = 1;
    }
    pPlc->modbusLength = modbusLength;
    pPlc->pollDelay = pollMsec/1000.;
    pPlc->histogramMsPerBin = 1;

    /* Set readback offset for Wago devices for which the register readback address 
     * is different from the register write address */
    if (strstr(pPlc->plcType, WAGO_ID_STRING) != NULL) 
        pPlc->readbackOffset = WAGO_OFFSET;

    switch(pPlc->modbusFunction) {
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
            IOLength = pPlc->modbusLength/16;
            maxLength = MAX_READ_WORDS;
            needReadThread = 1;
            break;
        case MODBUS_READ_HOLDING_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS_F23:
            IOLength = pPlc->modbusLength;
            maxLength = MAX_READ_WORDS;
            needReadThread = 1;
            break;
        case MODBUS_WRITE_SINGLE_COIL:
        case MODBUS_WRITE_MULTIPLE_COILS:
            IOLength = pPlc->modbusLength/16;
            maxLength = MAX_WRITE_WORDS;
            pPlc->readOnceFunction = MODBUS_READ_COILS;
            break;
       case MODBUS_WRITE_SINGLE_REGISTER:
       case MODBUS_WRITE_MULTIPLE_REGISTERS:
            IOLength = pPlc->modbusLength;
            maxLength = MAX_WRITE_WORDS;
            pPlc->readOnceFunction = MODBUS_READ_HOLDING_REGISTERS;
            break;
       case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
            IOLength = pPlc->modbusLength;
            maxLength = MAX_WRITE_WORDS;
            pPlc->readOnceFunction = MODBUS_READ_INPUT_REGISTERS_F23;
            break;
       default:
            errlogPrintf("%s::drvModbusAsynConfig port %s unsupported"
                         " Modbus function %d\n",
                         driver, pPlc->portName, pPlc->modbusFunction);
            return asynError;
    }
 
    /* If we are using absolute addressing then don't start read thread */
    if (pPlc->absoluteAddressing) needReadThread = 0;

    /* Make sure memory length is valid. */
    if (pPlc->modbusLength <= 0) {
        errlogPrintf("%s::drvModbusConfigure, port %s" 
                     " memory length<=0\n",
                     driver, pPlc->portName);
        return asynError;
    }
    if (IOLength > maxLength) {
        errlogPrintf("%s::drvModbusConfigure, port %s" 
                     " memory length=%d too large, max=%d\n",
                     driver, pPlc->portName, IOLength, maxLength);
        return asynError;
    }
    
    /* Note that we always allocate modbusLength words of memory.  
     * This is needed even for write operations because we need a buffer to convert
     * data for asynInt32Array writes. */
    pPlc->data = callocMustSucceed(pPlc->modbusLength, sizeof(epicsUInt16), 
                                   "drvModbusAsynConfigure");

    /* Connect to asyn octet port with asynOctetSyncIO */
    status = pasynOctetSyncIO->connect(octetPortName, 0, &pPlc->pasynUserOctet, 0);
    if (status != asynSuccess) {
        errlogPrintf("%s::drvModbusAsynConfigure port %s"
                     " can't connect to asynOctet on Octet server %s.\n",
                     driver, portName, octetPortName);
        return asynError;
    }

    /* Connect to asyn octet port with asynCommonSyncIO */
    status = pasynCommonSyncIO->connect(octetPortName, 0, &pPlc->pasynUserCommon, 0);
    if (status != asynSuccess) {
        errlogPrintf("%s::drvModbusAsynConfigure port %s"
                     " can't connect to asynCommon on Octet server %s.\n",
                     driver, portName, octetPortName);
        return asynError;
    }

    /* Create asynUser for asynTrace */
    pPlc->pasynUserTrace = pasynManager->createAsynUser(0, 0);
    pPlc->pasynUserTrace->userPvt = pPlc;

    status = pasynManager->registerPort(pPlc->portName,
                                        ASYN_MULTIDEVICE | ASYN_CANBLOCK,
                                        1, /* autoconnect */
                                        0, /* medium priority */
                                        0); /* default stack size */
    if (status != asynSuccess) {
        errlogPrintf("%s::drvModbusAsynConfigure port %s"
                     " can't register port\n",
                     driver, pPlc->portName);
        return asynError;
    }

    /* Create asyn interfaces and register with asynManager */
    pPlc->asynStdInterfaces.common.pinterface          = (void *)&drvCommon;
    pPlc->asynStdInterfaces.drvUser.pinterface         = (void *)&drvUser;
    pPlc->asynStdInterfaces.uInt32Digital.pinterface   = (void *)&drvUInt32D;
    pPlc->asynStdInterfaces.int32.pinterface           = (void *)&drvInt32;
    pPlc->asynStdInterfaces.float64.pinterface         = (void *)&drvFloat64;
    pPlc->asynStdInterfaces.int32Array.pinterface      = (void *)&drvInt32Array;
    pPlc->asynStdInterfaces.octet.pinterface           = (void *)&drvOctet;
    pPlc->asynStdInterfaces.uInt32DigitalCanInterrupt  = 1;
    pPlc->asynStdInterfaces.int32CanInterrupt          = 1;
    pPlc->asynStdInterfaces.float64CanInterrupt        = 1;
    pPlc->asynStdInterfaces.int32ArrayCanInterrupt     = 1;
    pPlc->asynStdInterfaces.octetCanInterrupt          = 1;


    status = pasynStandardInterfacesBase->initialize(pPlc->portName, &pPlc->asynStdInterfaces,
                                                     pPlc->pasynUserTrace, pPlc);
    if (status != asynSuccess) {
        errlogPrintf("%s::drvModbusAsynConfigure port %s"
                     " can't register standard interfaces: %s\n",
                     driver, pPlc->portName, pPlc->pasynUserTrace->errorMessage);
        return asynError;
    }

    /* Connect to device */
    status = pasynManager->connectDevice(pPlc->pasynUserTrace, pPlc->portName, 0);
    if (status != asynSuccess) {
        errlogPrintf("%s::drvModbusAsynConfigure port %s"
                     " connectDevice failed %s\n",
                     driver, pPlc->portName, pPlc->pasynUserTrace->errorMessage);
         return asynError;
    }
    
    /* If this is an output function do a readOnce operation if required. */
    if (pPlc->readOnceFunction && !pPlc->absoluteAddressing && (pPlc->pollDelay != 0)) {
         status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->readOnceFunction, 
                            (pPlc->modbusStartAddress + pPlc->readbackOffset), 
                            pPlc->data, pPlc->modbusLength);
        if (status == asynSuccess) pPlc->readOnceDone = 1;
    }
    
    /* Create the epicsEvent to wake up the readPoller.  
     * We do this even if there is no poller. */
    pPlc->readPollerEventId = epicsEventCreate(epicsEventEmpty);
     
    /* Create the thread to read registers if this is a read function code */
    if (needReadThread) {
        epicsSnprintf(readThreadName, 100, "%sRead", pPlc->portName);
        pPlc->readPollerThreadId = epicsThreadCreate(readThreadName,
           epicsThreadPriorityMedium,
           epicsThreadGetStackSize(epicsThreadStackSmall),
           (EPICSTHREADFUNC)readPoller, 
           pPlc);
        pPlc->forceCallback = 1;
    }

    /* Register for epicsAtExit callback.  Only need to do this once per IOC */
    if (!modbusInitialized) {
        modbusInitialized = 1;
        epicsAtExit(modbusExitCallback, pPlc);
    }
    return asynSuccess;
}


/* asynDrvUser routines */
static asynStatus drvUserCreate(void *drvPvt, asynUser *pasynUser,
                                const char *drvInfo,
                                const char **pptypeName, size_t *psize)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    int i;
    int offset;
    char *pstring;
    
    /* We are passed a string that identifies this command.
     * Set dataType and/or pasynUser->reason based on this string */

    pasynUser->drvUser = &pPlc->dataType;
    for (i=0; i<MAX_MODBUS_DATA_TYPES; i++) {
        pstring = modbusDataTypes[i].dataTypeString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(pPlc, offset)) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::drvUserCreate port %s invalid offset %d\n",
                          driver, pPlc->portName, offset);
                return asynError;
            }
            pasynUser->drvUser = &modbusDataTypes[i].dataType;
            pasynUser->reason = modbusDataCommand;
            if (pptypeName) *pptypeName = epicsStrDup(MODBUS_DATA_STRING);
            if (psize) *psize = sizeof(MODBUS_DATA_STRING);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                      "%s::drvUserCreate, port %s data type=%s\n", 
                      driver, pPlc->portName, pstring);
            return asynSuccess;
        }
    }

    for (i=0; i<MAX_MODBUS_COMMANDS; i++) {
        pstring = modbusCommands[i].commandString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynUser->reason = modbusCommands[i].command;
            if (pptypeName) *pptypeName = epicsStrDup(pstring);
            if (psize) *psize = sizeof(modbusCommands[i].command);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                      "%s::drvUserCreate, port %s command=%s\n", 
                      driver, pPlc->portName, pstring);
            return asynSuccess;
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s::drvUserCreate, port %s unknown command=%s\n", 
              driver, pPlc->portName, drvInfo);
    return asynError;
}

static asynStatus drvUserGetType(void *drvPvt, asynUser *pasynUser,
                                 const char **pptypeName, size_t *psize)
{
    int command = pasynUser->reason;

    if (pptypeName)
        *pptypeName = epicsStrDup(modbusCommands[command].commandString);
    if (psize) *psize = sizeof(command);
    return asynSuccess;
}

static asynStatus drvUserDestroy(void *drvPvt, asynUser *pasynUser)
{
    return asynSuccess;
}


/***********************/
/* asynCommon routines */
/***********************/

/* Connect */
static asynStatus asynConnect(void *drvPvt, asynUser *pasynUser)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    int offset;
  
    pasynManager->getAddr(pasynUser, &offset);
    if (offset < -1) return asynError;
    if (pPlc->absoluteAddressing) {
        if (offset > 65535) return asynError;
    } else {
        if (offset >= pPlc->modbusLength) return asynError;
    }
    pasynManager->exceptionConnect(pasynUser);
    return asynSuccess;
}

/* Disconnect */
static asynStatus asynDisconnect(void *drvPvt, asynUser *pasynUser)
{
    /* Does nothing for now.  
     * May be used if connection management is implemented */
    pasynManager->exceptionDisconnect(pasynUser);
    return asynSuccess;
}


/* Report  parameters */
static void asynReport(void *drvPvt, FILE *fp, int details)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;

    fprintf(fp, "modbus port: %s\n", pPlc->portName);
    if (details) {
        fprintf(fp, "    asynOctet server:   %s\n", pPlc->octetPortName);
        fprintf(fp, "    modbusSlave:        %d\n", pPlc->modbusSlave);
        fprintf(fp, "    modbusFunction:     %d\n", pPlc->modbusFunction);
        fprintf(fp, "    modbusStartAddress: 0%o\n", pPlc->modbusStartAddress);
        fprintf(fp, "    modbusLength:       0%o\n", pPlc->modbusLength);
        fprintf(fp, "    absoluteAddressing: %d\n", pPlc->absoluteAddressing);
        fprintf(fp, "    plcType:            %s\n", pPlc->plcType);
        fprintf(fp, "    I/O errors:         %d\n", pPlc->IOErrors);
        fprintf(fp, "    Read OK:            %d\n", pPlc->readOK);
        fprintf(fp, "    Write OK:           %d\n", pPlc->writeOK);
        fprintf(fp, "    pollDelay:          %f\n", pPlc->pollDelay);
        fprintf(fp, "    Time for last I/O   %d msec\n", pPlc->lastIOMsec);
        fprintf(fp, "    Max. I/O time:      %d msec\n", pPlc->maxIOMsec);
        fprintf(fp, "    Time per hist. bin: %d msec\n", pPlc->histogramMsPerBin);
    }

}


/* 
**  asynUInt32D support
*/
static asynStatus readUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 *value,
                              epicsUInt32 mask)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    int offset;
    int modbusFunction;

    switch(pasynUser->reason) {
        case modbusDataCommand:
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(pPlc, offset)) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::readUInt32D port %s invalid offset %d\n",
                          driver, pPlc->portName, offset);
                return asynError;
            }
            if (pPlc->absoluteAddressing) {
                /* If absolute addressing then there is no poller running */
                if (checkModbusFunction(pPlc, &modbusFunction)) return asynError;
                pPlc->ioStatus = doModbusIO(pPlc, pPlc->modbusSlave, modbusFunction,
                                            offset, pPlc->data, pPlc->modbusLength);
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
                offset = 0;
                pPlc->readOnceDone = 1;
            } else {
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
            }
            *value = 0;
            switch(pPlc->modbusFunction) {
                case MODBUS_READ_COILS:
                case MODBUS_READ_DISCRETE_INPUTS:
                case MODBUS_READ_HOLDING_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS_F23:
                    *value = pPlc->data[offset];
                    if ((mask != 0 ) && (mask != 0xFFFF)) *value &= mask;
                    break;
                case MODBUS_WRITE_SINGLE_COIL:
                case MODBUS_WRITE_MULTIPLE_COILS:
                case MODBUS_WRITE_SINGLE_REGISTER:
                case MODBUS_WRITE_MULTIPLE_REGISTERS:
                case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                    if (!pPlc->readOnceDone) return asynError;
                    *value = pPlc->data[offset];
                    if ((mask != 0 ) && (mask != 0xFFFF)) *value &= mask;
                    break;
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readUInt32D port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER,
                      "%s::readUInt32D port %s function=0x%x,"
                      " offset=0%o, mask=0x%x, value=0x%x\n",
                      driver, pPlc->portName, pPlc->modbusFunction, 
                      offset, mask, *value);
            break;
        case modbusEnableHistogramCommand:
            *value = pPlc->enableHistogram;
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::writeUInt32D port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
    }

    return asynSuccess;
}


static asynStatus writeUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 value,
                               epicsUInt32 mask)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    int offset;
    int modbusAddress;
    int i;
    epicsUInt16 data = value;
    asynStatus status;
 
    switch(pasynUser->reason) {
        case modbusDataCommand:
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(pPlc, offset)) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::writeUInt32D port %s invalid offset %d\n",
                          driver, pPlc->portName, offset);
                return asynError;
            }
            if (pPlc->absoluteAddressing) {
                modbusAddress = offset;
            } else {
                modbusAddress = pPlc->modbusStartAddress + offset;
            }
            switch(pPlc->modbusFunction) {
                case MODBUS_WRITE_SINGLE_COIL:
                    status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction, 
                                        modbusAddress, &data, 1);
                    if (status != asynSuccess) return(status);
                    break;
                case MODBUS_WRITE_SINGLE_REGISTER:
                    /* Do this as a read/modify/write if mask is not all 0 or all 1 */
                    if ((mask == 0) || (mask == 0xFFFF)) {
                        status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                            modbusAddress, &data, 1);
                    } else {
                        status = doModbusIO(pPlc, pPlc->modbusSlave, MODBUS_READ_HOLDING_REGISTERS,
                                            modbusAddress + pPlc->readbackOffset, &data, 1);
                        if (status != asynSuccess) return(status);
                        /* Set bits that are set in the value and set in the mask */
                        data |=  (value & mask);
                        /* Clear bits that are clear in the value and set in the mask */
                        data  &= (value | ~mask);
                        status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                            modbusAddress, &data, 1);
                    }
                    if (status != asynSuccess) return(status);
                    break;
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::writeUInt32D port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER,
                      "%s::writeUInt32D port %s function=0x%x,"
                      " address=0%o, mask=0x%x, value=0x%x\n",
                      driver, pPlc->portName, pPlc->modbusFunction, 
                      modbusAddress, mask, data);
            break;
        case modbusEnableHistogramCommand:
            if ((value != 0) && pPlc->enableHistogram == 0) {
                /* We are turning on histogram enabling, erase existing data first */
                for (i=0; i<HISTOGRAM_LENGTH; i++) {
                    pPlc->timeHistogram[i] = 0;
                }
            }
            pPlc->enableHistogram = value ? 1 : 0;
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::writeUInt32D port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
    }
           
    return asynSuccess;
}


/* setInterrupt, clearInterrupt, and getInterrupt are required by the asynUInt32Digital
 * interface.
 * They are used to control hardware interrupts for drivers that use them.  
 * They are not needed for Modbus, so we just return asynSuccess. */
 
static asynStatus setInterrupt (void *drvPvt, asynUser *pasynUser, epicsUInt32 mask, 
                                interruptReason reason)
{
    return asynSuccess;
}
                                
static asynStatus clearInterrupt (void *drvPvt, asynUser *pasynUser, epicsUInt32 mask)
{
    return asynSuccess;
}

static asynStatus getInterrupt (void *drvPvt, asynUser *pasynUser, epicsUInt32 *mask, 
                                interruptReason reason)
{
    return asynSuccess;
}
                                


/* 
**  asynInt32 support
*/

static asynStatus readInt32 (void *drvPvt, asynUser *pasynUser, epicsInt32 *value)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    modbusDataType_t dataType = getDataType(pPlc, pasynUser);
    int offset;
    asynStatus status;
    int bufferLen;
    int modbusFunction;
    
    *value = 0;
    
    switch(pasynUser->reason) {
        case modbusDataCommand:
                pasynManager->getAddr(pasynUser, &offset);
                if (checkOffset(pPlc, offset)) {
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readInt32 port %s invalid offset %d\n",
                              driver, pPlc->portName, offset);
                    return asynError;
                }
            if (pPlc->absoluteAddressing) {
                /* If absolute addressing then there is no poller running */
                if (checkModbusFunction(pPlc, &modbusFunction)) return asynError;
                pPlc->ioStatus = doModbusIO(pPlc, pPlc->modbusSlave, modbusFunction,
                                            offset, pPlc->data, pPlc->modbusLength);
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
                offset = 0;
                pPlc->readOnceDone = 1;
            } else {
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
            }
            switch(pPlc->modbusFunction) {
                case MODBUS_READ_COILS:
                case MODBUS_READ_DISCRETE_INPUTS:
                    *value = pPlc->data[offset];
                    break;
                case MODBUS_READ_HOLDING_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS_F23:
                    status = readPlcInt(pPlc, dataType, offset, value, &bufferLen);
                    if (status != asynSuccess) return status;
                    break;
                case MODBUS_WRITE_SINGLE_COIL:
                case MODBUS_WRITE_MULTIPLE_COILS:
                    if (!pPlc->readOnceDone) return asynError ;
                    *value = pPlc->data[offset];
                    break;
                case MODBUS_WRITE_SINGLE_REGISTER:
                case MODBUS_WRITE_MULTIPLE_REGISTERS:
                case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                    if (!pPlc->readOnceDone) return asynError;
                    status = readPlcInt(pPlc, dataType, offset, value, &bufferLen);
                    if (status != asynSuccess) return status;
                    break;
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readInt32 port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER,
                      "%s::readInt32 port %s function=0x%x,"
                      " offset=0%o, value=0x%x\n",
                      driver, pPlc->portName, pPlc->modbusFunction, 
                      offset, *value);
            break;
        case modbusReadCommand: 
            *value = 1;
            break;
        case modbusReadOKCommand: 
            *value = pPlc->readOK;
            break;
        case modbusWriteOKCommand: 
            *value = pPlc->writeOK;
            break;
        case modbusIOErrorsCommand: 
            *value = pPlc->IOErrors;
            break;
        case modbusLastIOTimeCommand: 
            *value = pPlc->lastIOMsec;
            break;
        case modbusMaxIOTimeCommand: 
            *value = pPlc->maxIOMsec;
            break;
        case modbusHistogramBinTimeCommand: 
            *value = pPlc->histogramMsPerBin;
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::readInt32 port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
        }
    
    return asynSuccess;
}


static asynStatus writeInt32(void *drvPvt, asynUser *pasynUser, epicsInt32 value)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    modbusDataType_t dataType = getDataType(pPlc, pasynUser);
    int offset;
    int modbusAddress;
    epicsUInt16 buffer[4];
    int bufferLen=0;
    int i;
    asynStatus status;


    switch(pasynUser->reason) {
        case modbusDataCommand:
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(pPlc, offset)) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::writeInt32 port %s invalid offset %d\n",
                          driver, pPlc->portName, offset);
                return asynError;
            }
            if (pPlc->absoluteAddressing) {
                modbusAddress = offset;
                offset = 0;
            } else {
                modbusAddress = pPlc->modbusStartAddress + offset;
            }
            switch(pPlc->modbusFunction) {
                case MODBUS_WRITE_SINGLE_COIL:
                    buffer[0] = value;
                    status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                        modbusAddress, buffer, 1);
                    if (status != asynSuccess) return(status);
                    break;
                case MODBUS_WRITE_SINGLE_REGISTER:
                    status = writePlcInt(pPlc, dataType, offset, value, buffer, &bufferLen);
                    if (status != asynSuccess) return(status);
                    for (i=0; i<bufferLen; i++) {
                        status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                            modbusAddress+i, buffer+i, bufferLen);
                    }
                    if (status != asynSuccess) return(status);
                    break;
                case MODBUS_WRITE_MULTIPLE_REGISTERS:
                case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                    status = writePlcInt(pPlc, dataType, offset, value, buffer, &bufferLen);
                    if (status != asynSuccess) return(status);
                    status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                        modbusAddress, buffer, bufferLen);
                    if (status != asynSuccess) return(status);
                    break;
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::writeInt32 port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER,
                      "%s::writeInt32 port %s function=0x%x,"
                      " modbusAddress=0%o, buffer[0]=0x%x, bufferLen=%d\n",
                      driver, pPlc->portName, pPlc->modbusFunction, 
                      modbusAddress, buffer[0], bufferLen);
            break;            
        case modbusReadCommand:
            /* Read the data for this driver.  This can be used when the poller is disabled. */
            epicsEventSignal(pPlc->readPollerEventId);
            break;
        case modbusHistogramBinTimeCommand:
            /* Set the time per histogram bin in ms */
            pPlc->histogramMsPerBin = value;
            if (pPlc->histogramMsPerBin < 1) pPlc->histogramMsPerBin = 1;
            /* Since the time might have changed erase existing data */
            for (i=0; i<HISTOGRAM_LENGTH; i++) {
                pPlc->timeHistogram[i] = 0;
                pPlc->histogramTimeAxis[i] = i * pPlc->histogramMsPerBin;
            }
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::writeInt32 port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
    }
    return asynSuccess;
}


/* This function should return the valid range for asynInt32 data.  
 * However, for Modbus devices there is no way for the driver to know what the 
 * valid range is, since the device could be an 8-bit unipolar ADC, 12-bit bipolar DAC,
 * etc.  Just return 0.
 * It is up to device support to correctly interpret the numbers that are returned. */
 
static asynStatus getBounds (void *drvPvt, asynUser *pasynUser, 
                             epicsInt32 *low, epicsInt32 *high)
{
    *high = 0;
    *low = 0;
    return asynSuccess;
}



/* 
**  asynFloat64 support
*/
static asynStatus readFloat64 (void *drvPvt, asynUser *pasynUser, epicsFloat64 *value)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    modbusDataType_t dataType = getDataType(pPlc, pasynUser);
    int offset;
    int bufferLen;
    int modbusFunction;
    asynStatus status = asynSuccess;
    
    *value = 0;
    
    switch(pasynUser->reason) {
        case modbusDataCommand:
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(pPlc, offset)) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::readFloat64 port %s invalid offset %d\n",
                          driver, pPlc->portName, offset);
                return asynError;
            }
            if (pPlc->absoluteAddressing) {
                /* If absolute addressing then there is no poller running */
                if (checkModbusFunction(pPlc, &modbusFunction)) return asynError;
                pPlc->ioStatus = doModbusIO(pPlc, pPlc->modbusSlave, modbusFunction,
                                            offset, pPlc->data, pPlc->modbusLength);
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
                offset = 0;
                pPlc->readOnceDone = 1;
            } else {
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
            }
            switch(pPlc->modbusFunction) {
                case MODBUS_READ_COILS:
                case MODBUS_READ_DISCRETE_INPUTS:
                    *value = pPlc->data[offset];
                     break;
                case MODBUS_READ_HOLDING_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS_F23:
                    status = readPlcFloat(pPlc, dataType, offset, value, &bufferLen);
                    if (status != asynSuccess) return status;
                    break;
                case MODBUS_WRITE_SINGLE_COIL:
                case MODBUS_WRITE_MULTIPLE_COILS:
                    if (!pPlc->readOnceDone) return asynError;
                    *value = pPlc->data[offset];
                    break;
                case MODBUS_WRITE_SINGLE_REGISTER:
                case MODBUS_WRITE_MULTIPLE_REGISTERS:
                case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                    if (!pPlc->readOnceDone) return asynError;
                    status = readPlcFloat(pPlc, dataType, offset, value, &bufferLen);
                    break;
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readFloat64 port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER,
                      "%s::readFloat64 port %s function=0x%x,"
                      " offset=0%o, value=%f, status=%d\n",
                      driver, pPlc->portName, pPlc->modbusFunction, 
                      offset, *value, status);
            break;
        case modbusPollDelayCommand:
            *value = pPlc->pollDelay;
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::readFloat64 port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
        }
    
    return asynSuccess;
}


static asynStatus writeFloat64 (void *drvPvt, asynUser *pasynUser, epicsFloat64 value)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    modbusDataType_t dataType = getDataType(pPlc, pasynUser);
    int offset;
    int modbusAddress;
    epicsUInt16 buffer[4];
    int bufferLen;
    int i;
    asynStatus status;


    switch(pasynUser->reason) {
        case modbusDataCommand:
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(pPlc, offset)) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::writeFloat64 port %s invalid offset %d\n",
                          driver, pPlc->portName, offset);
                return asynError;
            }
            if (pPlc->absoluteAddressing) {
                modbusAddress = offset;
                offset = 0;
            } else {
                modbusAddress = pPlc->modbusStartAddress + offset;
            }
            switch(pPlc->modbusFunction) {
                case MODBUS_WRITE_SINGLE_COIL:
                    buffer[0] = (epicsUInt16)value;
                    status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                         modbusAddress, buffer, 1);
                    if (status != asynSuccess) return(status);
                    break;
                case MODBUS_WRITE_SINGLE_REGISTER:
                    status = writePlcFloat(pPlc, dataType, offset, value, buffer, &bufferLen);
                    for (i=0; i<bufferLen; i++) {
                        status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                            modbusAddress+i, buffer+i, 1);
                        if (status != asynSuccess) return(status);
                    }
                    break;
                case MODBUS_WRITE_MULTIPLE_REGISTERS:
                case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                    status = writePlcFloat(pPlc, dataType, offset, value, buffer, &bufferLen);
                    status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                        modbusAddress, buffer, bufferLen);
                    if (status != asynSuccess) return(status);
                    break;
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::writeFloat64 port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER,
                      "%s::writeFloat64 port %s function=0x%x,"
                      " modbusAddress=0%o, buffer[0]=0x%x\n",
                      driver, pPlc->portName, pPlc->modbusFunction, 
                      modbusAddress, buffer[0]);
            break;
        case modbusPollDelayCommand:
            pPlc->pollDelay = value;
            /* Send an event to the poller, because it might have a long poll time, or
             * not be polling at all */
            epicsEventSignal(pPlc->readPollerEventId);
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::writeFloat64 port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
    }
    return asynSuccess;
}



/* 
**  asynInt32Array support
*/
static asynStatus readInt32Array (void *drvPvt, asynUser *pasynUser, epicsInt32 *data,
                                  size_t maxChans, size_t *nactual)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    modbusDataType_t dataType = getDataType(pPlc, pasynUser);
    int offset;
    int i;
    int bufferLen;
    int modbusFunction;
    asynStatus status;
    
    *nactual = 0;
    pasynManager->getAddr(pasynUser, &offset);
    switch(pasynUser->reason) {
        case modbusDataCommand:
            if (pPlc->absoluteAddressing) {
                /* If absolute addressing then there is no poller running */
                if (checkModbusFunction(pPlc, &modbusFunction)) return asynError;
                pPlc->ioStatus = doModbusIO(pPlc, pPlc->modbusSlave, modbusFunction,
                                            offset, pPlc->data, pPlc->modbusLength);
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
                offset = 0;
            } else {
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
            }
            switch(pPlc->modbusFunction) {
                case MODBUS_READ_COILS:
                case MODBUS_READ_DISCRETE_INPUTS:
                    for (i=0; i<maxChans && offset<pPlc->modbusLength; i++) {
                        data[i] = pPlc->data[offset];
                        offset++;
                    }
                    break;
                case MODBUS_READ_HOLDING_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS_F23:
                    for (i=0; i<maxChans && offset<pPlc->modbusLength; i++) {
                        status = readPlcInt(pPlc, dataType, offset, &data[i], &bufferLen);
                        if (status) return status;
                        offset += bufferLen;
                    }
                    break;
                    
                case MODBUS_WRITE_SINGLE_COIL:
                case MODBUS_WRITE_MULTIPLE_COILS:
                    if (!pPlc->readOnceDone) return asynError;
                    for (i=0; i<maxChans && offset<pPlc->modbusLength; i++) {
                        data[i] = pPlc->data[offset];
                        offset++;
                    }
                    break;
                case MODBUS_WRITE_SINGLE_REGISTER:
                case MODBUS_WRITE_MULTIPLE_REGISTERS:
                case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                    if (!pPlc->readOnceDone) return asynError;
                    for (i=0; i<maxChans && offset<pPlc->modbusLength; i++) {
                        status = readPlcInt(pPlc, dataType, offset, &data[i], &bufferLen);
                        if (status) return status;
                        offset += bufferLen;
                    }
                    break;
                    
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readInt32 port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)pPlc->data, i*2, 
                        "%s::readInt32Array port %s, function=0x%x\n",
                        driver, pPlc->portName, pPlc->modbusFunction);
            break;
            
        case modbusReadHistogramCommand:
            for (i=0; i<maxChans && i<HISTOGRAM_LENGTH; i++) {
                data[i] = pPlc->timeHistogram[i];
            }
            break;
        
        case modbusHistogramTimeAxisCommand:
            for (i=0; i<maxChans && i<HISTOGRAM_LENGTH; i++) {
                data[i] = pPlc->histogramTimeAxis[i];
            }
            break;
        
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::readInt32 port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
        }
    
    *nactual = i;
    return asynSuccess;
}


static asynStatus writeInt32Array (void *drvPvt, asynUser *pasynUser, epicsInt32 *data,
                                   size_t maxChans)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    modbusDataType_t dataType = getDataType(pPlc, pasynUser);
    int modbusAddress;
    epicsUInt16 *dataAddress;
    int nwrite=0;
    int i;
    int offset;
    int outIndex;
    int bufferLen;
    asynStatus status;

    pasynManager->getAddr(pasynUser, &offset);
    if (pPlc->absoluteAddressing) {
        modbusAddress = offset;
        dataAddress = pPlc->data;
        outIndex = 0;
    } else {
        modbusAddress = pPlc->modbusStartAddress + offset;
        dataAddress = pPlc->data + offset;
        outIndex = offset;
    }
    switch(pasynUser->reason) {
        case modbusDataCommand:
            switch(pPlc->modbusFunction) {
                case MODBUS_WRITE_MULTIPLE_COILS:
                    /* Need to copy data to local buffer to convert to epicsUInt16 */
                    for (i=0; i<maxChans && outIndex<pPlc->modbusLength; i++) {
                        pPlc->data[outIndex] = data[i];
                        outIndex++;
                        nwrite++;
                    }
                    status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                        modbusAddress, dataAddress, nwrite);
                    if (status != asynSuccess) return(status);
                    break;
                case MODBUS_WRITE_MULTIPLE_REGISTERS:
                case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                    for (i=0; i<maxChans && outIndex<pPlc->modbusLength; i++) {
                        status = writePlcInt(pPlc, dataType, outIndex, data[i], &pPlc->data[outIndex], &bufferLen);
                        if (status != asynSuccess) return(status);
                        outIndex += bufferLen;
                        nwrite += bufferLen;
                    }
                    status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                        modbusAddress, dataAddress, nwrite);
                    if (status != asynSuccess) return(status);
                    break;
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::writeInt32Array port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)pPlc->data, i*2, 
                        "%s::writeInt32Array port %s, function=0x%x\n",
                        driver, pPlc->portName, pPlc->modbusFunction);
            break;
            
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::writeInt32Array port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
    }
    return asynSuccess;
}


/* 
**  asynOctet support
*/
static asynStatus readOctet (void *drvPvt, asynUser *pasynUser, char *data,
                             size_t maxChars, size_t *nactual, int *eomReason)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    modbusDataType_t dataType = getDataType(pPlc, pasynUser);
    int offset;
    int bufferLen;
    int modbusFunction;
    
    *nactual = 0;
    pasynManager->getAddr(pasynUser, &offset);
    switch(pasynUser->reason) {
        case modbusDataCommand:
            if (pPlc->absoluteAddressing) {
                /* If absolute addressing then there is no poller running */
                if (checkModbusFunction(pPlc, &modbusFunction)) return asynError;
                pPlc->ioStatus = doModbusIO(pPlc, pPlc->modbusSlave, modbusFunction,
                                            offset, pPlc->data, pPlc->modbusLength);
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
                offset = 0;
            } else {
                if (pPlc->ioStatus != asynSuccess) return(pPlc->ioStatus);
            }
            switch(pPlc->modbusFunction) {
                case MODBUS_WRITE_MULTIPLE_REGISTERS:
                case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                    if (!pPlc->readOnceDone) return asynError;
                case MODBUS_READ_HOLDING_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS:
                case MODBUS_READ_INPUT_REGISTERS_F23:
                    readPlcString(pPlc, dataType, offset, data, maxChars, &bufferLen);
                    *nactual = bufferLen;
                    *eomReason = ASYN_EOM_CNT;
                    break;
                    
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readInt32 port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)pPlc->data, *nactual, 
                        "%s::readOctet port %s, function=0x%x\n",
                        driver, pPlc->portName, pPlc->modbusFunction);
            break;
            
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::readOctet port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
        }

    return asynSuccess;
}


static asynStatus writeOctet (void *drvPvt, asynUser *pasynUser, const char *data, size_t maxChars, size_t *nActual)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    modbusDataType_t dataType = getDataType(pPlc, pasynUser);
    int modbusAddress;
    epicsUInt16 *dataAddress;
    int offset;
    int bufferLen;
    asynStatus status;

    pasynManager->getAddr(pasynUser, &offset);
    if (pPlc->absoluteAddressing) {
        modbusAddress = offset;
        dataAddress = pPlc->data;
    } else {
        modbusAddress = pPlc->modbusStartAddress + offset;
        dataAddress = pPlc->data + offset;
    }
    switch(pasynUser->reason) {
        case modbusDataCommand:
            switch(pPlc->modbusFunction) {
                case MODBUS_WRITE_MULTIPLE_REGISTERS:
                case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
                    writePlcString(pPlc, dataType, offset, data, maxChars, nActual, &bufferLen);
                    if (bufferLen <= 0) break;
                    status = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                        modbusAddress, dataAddress, bufferLen);
                    if (status != asynSuccess) return(status);
                    break;
                default:
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::writeOctet port %s invalid request for Modbus"
                              " function %d\n",
                              driver, pPlc->portName, pPlc->modbusFunction);
                    return asynError;
            }
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)pPlc->data, bufferLen*2, 
                        "%s::writeOctet port %s, function=0x%x\n",
                        driver, pPlc->portName, pPlc->modbusFunction);
            break;
            
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::writeInt32Array port %s invalid pasynUser->reason %d\n",
                      driver, pPlc->portName, pasynUser->reason);
            return asynError;
    }
    return asynSuccess;
}


/*
****************************************************************************
** Poller thread for port reads
   One instance spawned per asyn port
****************************************************************************
*/

static void readPoller(PLC_ID pPlc)
{

    ELLLIST *pclientList;
    interruptNode *pnode;
    asynUInt32DigitalInterrupt *pUInt32D;
    asynInt32Interrupt *pInt32;
    asynFloat64Interrupt *pFloat64;
    asynInt32ArrayInterrupt *pInt32Array;
    asynOctetInterrupt *pOctet;
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

    prevData = callocMustSucceed(pPlc->modbusLength, sizeof(epicsUInt16), 
                                 "drvModbusAsyn::readPoller");
    int32Data = callocMustSucceed(pPlc->modbusLength, sizeof(epicsInt32), 
                                 "drvModbusAsyn::readPoller");
                            
    pasynManager->lockPort(pPlc->pasynUserTrace);

    /* Loop forever */    
    while (1)
    {
        /* Sleep for the poll delay or waiting for epicsEvent with the port unlocked */
        pasynManager->unlockPort(pPlc->pasynUserTrace);
        if (pPlc->pollDelay > 0.0) {
            epicsEventWaitWithTimeout(pPlc->readPollerEventId, pPlc->pollDelay);
        } else {
            epicsEventWait(pPlc->readPollerEventId);
        }

        if (modbusExiting) break;

        /* Lock the port.  It is important that the port be locked so other threads cannot access the pPlc
         * structure while the poller thread is running. */
        pasynManager->lockPort(pPlc->pasynUserTrace);
         
        /* Read the data */
        pPlc->ioStatus = doModbusIO(pPlc, pPlc->modbusSlave, pPlc->modbusFunction,
                                    pPlc->modbusStartAddress, pPlc->data, pPlc->modbusLength);
        /* If we have an I/O error this time and the previous time, just try again */
        if (pPlc->ioStatus != asynSuccess &&
            pPlc->ioStatus == prevIOStatus) {
            epicsThreadSleep(1.0);
            continue;
        }

        /* If the I/O status has changed then force callbacks */
        if (pPlc->ioStatus != prevIOStatus) pPlc->forceCallback = 1;
        
        /* See if any memory location has actually changed.  
         * If not, no need to do callbacks. */
        anyChanged = memcmp(pPlc->data, prevData, 
                            pPlc->modbusLength*sizeof(epicsUInt16));
 
        /* Don't start polling until EPICS interruptAccept flag is set, 
         * because it does callbacks to device support. */
        while (!interruptAccept) {
            pasynManager->unlockPort(pPlc->pasynUserTrace);
            epicsThreadSleep(0.1);
            pasynManager->lockPort(pPlc->pasynUserTrace);
        }

        /* Process callbacks to device support. */

        /* See if there are any asynUInt32Digital callbacks registered to be called
         * when data changes.  These callbacks only happen if the value has changed */
        if (pPlc->forceCallback || anyChanged){
            pasynManager->interruptStart(pPlc->asynStdInterfaces.uInt32DigitalInterruptPvt, &pclientList);
            pnode = (interruptNode *)ellFirst(pclientList);
            while (pnode) {
                pUInt32D = pnode->drvPvt;
                pasynUser = pUInt32D->pasynUser;
                if (pasynUser->reason != modbusDataCommand) {
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readPoller port %s invalid pasynUser->reason %d\n",
                              driver, pPlc->portName, pasynUser->reason);
                    break;
                }
                pasynManager->getAddr(pasynUser, &offset);
                if (checkOffset(pPlc, offset)) {
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readPoller port %s invalid offset %d, max=%d\n",
                              driver, pPlc->portName, offset, pPlc->modbusLength);
                    break;
                }
                mask = pUInt32D->mask;
                newValue = pPlc->data[offset];
                if ((mask != 0 ) && (mask != 0xFFFF)) newValue &= mask;
                prevValue = prevData[offset];
                if ((mask != 0 ) && (mask != 0xFFFF)) prevValue &= mask;
                /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
                pasynUser->auxStatus = pPlc->ioStatus;
                if (pPlc->forceCallback || (newValue != prevValue)) {
                    uInt32Value = newValue;
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_FLOW,
                              "%s::readPoller, calling client %p"
                              " mask=0x%x, callback=%p, data=0x%x\n",
                              driver, pUInt32D, pUInt32D->mask, pUInt32D->callback, uInt32Value);
                    pUInt32D->callback(pUInt32D->userPvt, pasynUser, uInt32Value);
                }
                pnode = (interruptNode *)ellNext(&pnode->node);
            }
            pasynManager->interruptEnd(pPlc->asynStdInterfaces.uInt32DigitalInterruptPvt);
        }
                
        /* See if there are any asynInt32 callbacks registered to be called. 
         * These are called even if the data has not changed, because we could be doing
         * ADC averaging */
        pasynManager->interruptStart(pPlc->asynStdInterfaces.int32InterruptPvt, &pclientList);
        pnode = (interruptNode *)ellFirst(pclientList);
        while (pnode) {
            pInt32 = pnode->drvPvt;
            pasynUser = pInt32->pasynUser;
            if (pasynUser->reason != modbusDataCommand) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::readPoller port %s invalid pasynUser->reason %d\n",
                          driver, pPlc->portName, pasynUser->reason);
                break;
            }
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(pPlc, offset)) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::readPoller port %s invalid memory request %d, max=%d\n",
                          driver, pPlc->portName, offset, pPlc->modbusLength);
                break;
            }
            dataType = getDataType(pPlc, pasynUser);
            readPlcInt(pPlc, dataType, offset, &int32Value, &bufferLen);
            /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
            pasynUser->auxStatus = pPlc->ioStatus;
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_FLOW,
                      "%s::readPoller, calling client %p"
                      "callback=%p, data=0x%x\n",
                      driver, pInt32, pInt32->callback, int32Value);
            pInt32->callback(pInt32->userPvt, pasynUser,
                             int32Value);
            pnode = (interruptNode *)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(pPlc->asynStdInterfaces.int32InterruptPvt);
 
        /* See if there are any asynFloat64 callbacks registered to be called.
         * These are called even if the data has not changed, because we could be doing
         * ADC averaging */
        pasynManager->interruptStart(pPlc->asynStdInterfaces.float64InterruptPvt, &pclientList);
        pnode = (interruptNode *)ellFirst(pclientList);
        while (pnode) {
            pFloat64 = pnode->drvPvt;
            pasynUser = pFloat64->pasynUser;
            if (pasynUser->reason != modbusDataCommand) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::readPoller port %s invalid pasynUser->reason %d\n",
                          driver, pPlc->portName, pasynUser->reason);
                break;
            }
            pasynManager->getAddr(pasynUser, &offset);
            if (checkOffset(pPlc, offset)) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::readPoller port %s invalid memory request %d, max=%d\n",
                          driver, pPlc->portName, offset, pPlc->modbusLength);
                break;
            }
            dataType = getDataType(pPlc, pasynUser);
            readPlcFloat(pPlc, dataType, offset, &float64Value, &bufferLen);
            /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
            pFloat64->pasynUser->auxStatus = pPlc->ioStatus;
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_FLOW,
                      "%s::readPoller, calling client %p"
                      "callback=%p, data=%f\n",
                      driver, pFloat64, pFloat64->callback, float64Value);
            pFloat64->callback(pFloat64->userPvt, pasynUser,
                               float64Value);
            pnode = (interruptNode *)ellNext(&pnode->node);
        }
        pasynManager->interruptEnd(pPlc->asynStdInterfaces.float64InterruptPvt);
        
       
        /* See if there are any asynInt32Array callbacks registered to be called.
         * These are only called when data changes */
        if (pPlc->forceCallback || anyChanged){
            pasynManager->interruptStart(pPlc->asynStdInterfaces.int32ArrayInterruptPvt, &pclientList);
            pnode = (interruptNode *)ellFirst(pclientList);
            while (pnode) {
                pInt32Array = pnode->drvPvt;
                pasynUser = pInt32Array->pasynUser;
                if (pasynUser->reason != modbusDataCommand) {
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readPoller port %s invalid pasynUser->reason %d\n",
                              driver, pPlc->portName, pasynUser->reason);
                    break;
                }
                /* Need to copy data to epicsInt32 buffer for callback */
                pasynManager->getAddr(pasynUser, &offset);
                dataType = getDataType(pPlc, pasynUser);
                for (i=0; i<pPlc->modbusLength && offset < pPlc->modbusLength; i++) {
                    readPlcInt(pPlc, dataType, offset, &int32Data[i], &bufferLen);
                    offset += bufferLen;
                }
                /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
                pasynUser->auxStatus = pPlc->ioStatus;
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_FLOW,
                          "%s::readPoller, calling client %p"
                          "callback=%p\n",
                           driver, pInt32Array, pInt32Array->callback);
                pInt32Array->callback(pInt32Array->userPvt, pasynUser,
                                      int32Data, i);
                pnode = (interruptNode *)ellNext(&pnode->node);
            }
            pasynManager->interruptEnd(pPlc->asynStdInterfaces.int32ArrayInterruptPvt);
        }

        /* See if there are any asynOctet callbacks registered to be called
         * when data changes.  These callbacks only happen if any data in this port has changed */
        if (pPlc->forceCallback || anyChanged){
            pasynManager->interruptStart(pPlc->asynStdInterfaces.octetInterruptPvt, &pclientList);
            pnode = (interruptNode *)ellFirst(pclientList);
            while (pnode) {
                pOctet = pnode->drvPvt;
                pasynUser = pOctet->pasynUser;
                if (pasynUser->reason != modbusDataCommand) {
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readPoller port %s invalid pasynUser->reason %d\n",
                              driver, pPlc->portName, pasynUser->reason);
                    break;
                }
                pasynManager->getAddr(pasynUser, &offset);
                dataType = getDataType(pPlc, pasynUser);
                if (checkOffset(pPlc, offset)) {
                    asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                              "%s::readPoller port %s invalid offset %d, max=%d\n",
                              driver, pPlc->portName, offset, pPlc->modbusLength);
                    break;
                }
                readPlcString(pPlc, dataType, offset, stringBuffer, sizeof(stringBuffer), &bufferLen);
                /* Set the status flag in pasynUser so I/O Intr scanned records can set alarm status */
                pasynUser->auxStatus = pPlc->ioStatus;
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_FLOW,
                          "%s::readPoller, calling client %p"
                          " callback=%p, data=%s\n",
                          driver, pOctet, pOctet->callback, stringBuffer);
                pOctet->callback(pOctet->userPvt, pasynUser, stringBuffer, bufferLen, ASYN_EOM_CNT);
                pnode = (interruptNode *)ellNext(&pnode->node);
            }
            pasynManager->interruptEnd(pPlc->asynStdInterfaces.octetInterruptPvt);
        }
                
        /* Reset the forceCallback flag */
        pPlc->forceCallback = 0;

        /* Set the previous I/O status */
        prevIOStatus = pPlc->ioStatus;

        /* Copy the new data to the previous data */
        memcpy(prevData, pPlc->data, pPlc->modbusLength*sizeof(epicsUInt16));
    }
}



static int doModbusIO(PLC_ID pPlc, int slave, int function, int start, 
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
 
    /* If the Octet driver is not set for autoConnect then do connection management ourselves */
    status = pasynManager->isAutoConnect(pPlc->pasynUserOctet, &autoConnect);
    if (!autoConnect) {
        /* See if we are connected */
        status = pasynManager->isConnected(pPlc->pasynUserOctet, &pPlc->isConnected);
         /* If we have an I/O error or are disconnected then disconnect device and reconnect */
        if ((pPlc->ioStatus != asynSuccess) || !pPlc->isConnected) {
            if (pPlc->ioStatus != asynSuccess) 
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR, 
                          "%s::doModbusIO port %s has I/O error\n",
                          driver, pPlc->portName);
            if (!pPlc->isConnected) 
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR, 
                          "%s::doModbusIO port %s is disconnected\n",
                          driver, pPlc->portName);
            status = pasynCommonSyncIO->disconnectDevice(pPlc->pasynUserCommon);
            if (status == asynSuccess) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_FLOW, 
                          "%s::doModbusIO port %s disconnect device OK\n",
                          driver, pPlc->portName);
            } else {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR, 
                          "%s::doModbusIO port %s disconnect error=%s\n",
                          driver, pPlc->portName, pPlc->pasynUserOctet->errorMessage);
            }
            status = pasynCommonSyncIO->connectDevice(pPlc->pasynUserCommon);
            if (status == asynSuccess) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_FLOW, 
                          "%s::doModbusIO port %s connect device OK\n",
                          driver, pPlc->portName);
            } else {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR, 
                          "%s::doModbusIO port %s connect device error=%s\n",
                          driver, pPlc->portName, pPlc->pasynUserOctet->errorMessage);
                goto done;
            }
        }
    }
        
    switch (function) {
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
            readReq = (modbusReadRequest *)pPlc->modbusRequest;
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
            readReq = (modbusReadRequest *)pPlc->modbusRequest;
            readReq->slave = slave;
            readReq->fcode = function;
            readReq->startReg = htons((epicsUInt16)start);
            readReq->numRead = htons((epicsUInt16)len);
            requestSize = sizeof(modbusReadRequest);
            /* The -1 below is because the modbusReadResponse struct already has 1 byte of data */
            replySize = sizeof(modbusReadResponse) - 1 + len*2;
            break;
        case MODBUS_READ_INPUT_REGISTERS_F23:
            readWriteMultipleReq = (modbusReadWriteMultipleRequest *)pPlc->modbusRequest;
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
            writeSingleReq = (modbusWriteSingleRequest *)pPlc->modbusRequest;
            writeSingleReq->slave = slave;
            writeSingleReq->fcode = function;
            writeSingleReq->startReg = htons((epicsUInt16)start);
            if (*data) bitOutput = 0xFF00;
            else       bitOutput = 0;
            writeSingleReq->data = htons(bitOutput);
            requestSize = sizeof(modbusWriteSingleRequest);
            replySize = sizeof(modbusWriteSingleResponse);
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                      "%s::doModbusIO port %s WRITE_SINGLE_COIL"
                      " address=0%o value=0x%x\n",
                      driver, pPlc->portName, start, bitOutput);
            break;
        case MODBUS_WRITE_SINGLE_REGISTER:
            writeSingleReq = (modbusWriteSingleRequest *)pPlc->modbusRequest;
            writeSingleReq->slave = slave;
            writeSingleReq->fcode = function;
            writeSingleReq->startReg = htons((epicsUInt16)start);
            writeSingleReq->data = (epicsUInt16)*data;
            writeSingleReq->data = htons(writeSingleReq->data);
            requestSize = sizeof(modbusWriteSingleRequest);
            replySize = sizeof(modbusWriteSingleResponse);
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                      "%s::doModbusIO port %s WRITE_SINGLE_REGISTER"
                      " address=0%o value=0x%x\n",
                      driver, pPlc->portName, start, *data);
            break;
        case MODBUS_WRITE_MULTIPLE_COILS:
            writeMultipleReq = (modbusWriteMultipleRequest *)pPlc->modbusRequest;
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
            byteCount = pCharOut - writeMultipleReq->data + 1;
            writeMultipleReq->byteCount = byteCount;
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)writeMultipleReq->data, byteCount, 
                        "%s::doModbusIO port %s WRITE_MULTIPLE_COILS\n",
                        driver, pPlc->portName);
            requestSize = sizeof(modbusWriteMultipleRequest) + byteCount - 1;
            replySize = sizeof(modbusWriteMultipleResponse);
            break;
        case MODBUS_WRITE_MULTIPLE_REGISTERS:
            writeMultipleReq = (modbusWriteMultipleRequest *)pPlc->modbusRequest;
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
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)writeMultipleReq->data, byteCount, 
                        "%s::doModbusIO port %s WRITE_MULTIPLE_REGISTERS\n",
                        driver, pPlc->portName);
            requestSize = sizeof(modbusWriteMultipleRequest) + byteCount - 1;
            replySize = sizeof(modbusWriteMultipleResponse);
            break;
        case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
            readWriteMultipleReq = (modbusReadWriteMultipleRequest *)pPlc->modbusRequest;
            readWriteMultipleReq->slave = slave;
            readWriteMultipleReq->fcode = MODBUS_READ_WRITE_MULTIPLE_REGISTERS;
            readWriteMultipleReq->startReadReg = htons((epicsUInt16)start);
            /* We don't actually do anything with the values read from the device, but it does not
             * seem to be allowed to specify numRead=0, so we always read one word from the same address
             * we write to. */
            nread = 1;
            readWriteMultipleReq->numRead = htons(nread);
            readWriteMultipleReq->startWriteReg = htons((epicsUInt16)start);
            pShortIn = (epicsUInt16 *)data;
            pShortOut = (epicsUInt16 *)&readWriteMultipleReq->data;
            for (i=0; i<len; i++, pShortOut++) {
                *pShortOut = htons(*pShortIn++);
            }
            readWriteMultipleReq->numOutput = htons(len);
            byteCount = 2*len;
            readWriteMultipleReq->byteCount = byteCount;
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)readWriteMultipleReq->data, byteCount, 
                        "%s::doModbusIO port %s WRITE_MULTIPLE_REGISTERS_F23\n",
                        driver, pPlc->portName);
            requestSize = sizeof(modbusReadWriteMultipleRequest) + byteCount - 1;
            /* The -1 below is because the modbusReadResponse struct already has 1 byte of data */
            replySize = sizeof(modbusReadResponse) + 2*nread - 1;
            break;


        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR, 
                      "%s::doModbusIO, port %s unsupported function code %d\n", 
                      driver, pPlc->portName, function);
            status = asynError;
            goto done;
    }

    /* Do the Modbus I/O as a write/read cycle */
    epicsTimeGetCurrent(&startTime);
    status = pasynOctetSyncIO->writeRead(pPlc->pasynUserOctet, 
                                         pPlc->modbusRequest, requestSize,
                                         pPlc->modbusReply, replySize,
                                         MODBUS_READ_TIMEOUT,
                                         &nwrite, &nread, &eomReason);
    epicsTimeGetCurrent(&endTime);

    if (status != pPlc->prevIOStatus) {                                      
      if (status != asynSuccess) {
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                     "%s::doModbusIO port %s error calling writeRead,"
                     " error=%s, nwrite=%d/%d, nread=%d\n", 
                     driver, pPlc->portName, 
                     pPlc->pasynUserOctet->errorMessage, (int)nwrite, requestSize, (int)nread);
        } else {
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                     "%s::doModbusIO port %s writeRead status back to normal"
                     " nwrite=%d/%d, nread=%d\n", 
                     driver, pPlc->portName, 
                     (int)nwrite, requestSize, (int)nread);
        }
        pPlc->prevIOStatus = status;
    }
    if (status != asynSuccess) {
        pPlc->IOErrors++;
        goto done;
    }

    dT = epicsTimeDiffInSeconds(&endTime, &startTime);
    msec = (int)(dT*1000. + 0.5);
    pPlc->lastIOMsec = msec;
    if (msec > pPlc->maxIOMsec) pPlc->maxIOMsec = msec;
    if (pPlc->enableHistogram) {
        bin = msec / pPlc->histogramMsPerBin;
        if (bin < 0) bin = 0;
        /* Longer times go in last bin of histogram */
        if (bin >= HISTOGRAM_LENGTH-1) bin = HISTOGRAM_LENGTH-1; 
        pPlc->timeHistogram[bin]++;
    }     

    /* See if there is a Modbus exception */
    readResp = (modbusReadResponse *)pPlc->modbusReply;
    if (readResp->fcode & MODBUS_EXCEPTION_FCN) {
        exceptionResp = (modbusExceptionResponse *)pPlc->modbusReply;
        asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                  "%s::doModbusIO port %s Modbus exception=%d\n", 
                  driver, pPlc->portName, exceptionResp->exception);
        status = asynError;
        goto done;
    }

    /* Make sure the function code in the response is the same as the one 
     * in the request? */

    switch (function) {
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
            pPlc->readOK++;
            readResp = (modbusReadResponse *)pPlc->modbusReply;
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
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)data, len*2, 
                        "%s::doModbusIO port %s READ_COILS\n",
                        driver, pPlc->portName);
            break;
        case MODBUS_READ_HOLDING_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS_F23:
            pPlc->readOK++;
            readResp = (modbusReadResponse *)pPlc->modbusReply;
            nread = readResp->byteCount/2;
            pShortIn = (epicsUInt16 *)&readResp->data;
            /* Check to make sure we got back the expected number of words */
            if (nread != len) {
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::doModbusIO, port %s expected %d words, actually received %d\n", 
                          driver, pPlc->portName, len, (int)nread);
                status = asynError;
                goto done;
            }
            for (i=0; i<(int)nread; i++) { 
                data[i] = ntohs(pShortIn[i]);
            }
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)data, nread*2, 
                        "%s::doModbusIO port %s READ_REGISTERS\n",
                        driver, pPlc->portName);
            break;

        /* We don't do anything with responses to writes for now.  
         * Could add error checking. */
        case MODBUS_WRITE_SINGLE_COIL:
        case MODBUS_WRITE_SINGLE_REGISTER:
            pPlc->writeOK++;
            /* Not using value for now so comment out to avoid compiler warning
            writeSingleResp = (modbusWriteSingleResponse *)pPlc->modbusReply;
            */
            break;
        case MODBUS_WRITE_MULTIPLE_COILS:
        case MODBUS_WRITE_MULTIPLE_REGISTERS:
            pPlc->writeOK++;
            /* Not using value for now so comment out to avoid compiler warning
            writeMultipleResp = (modbusWriteMultipleResponse *)pPlc->modbusReply;
            */
            break;
        case MODBUS_WRITE_MULTIPLE_REGISTERS_F23:
            pPlc->writeOK++;
            //readResp = (modbusReadResponse *)pPlc->modbusReply;
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::doModbusIO, port %s unsupported function code %d\n", 
                      driver, pPlc->portName, function);
            status = asynError;
            goto done;
    }

    done:
    return(status);
}

modbusDataType_t getDataType(PLC_ID pPlc, asynUser *pasynUser)
{
    modbusDataType_t dataType;
    if (pasynUser->drvUser) {
        dataType = *(modbusDataType_t *)pasynUser->drvUser;
    } else {
        dataType = pPlc->dataType;
    }
    return dataType;
}

int checkOffset(PLC_ID pPlc, int offset)
{
    if (offset < 0) return asynError;
    if (pPlc->absoluteAddressing) {
        if (offset > 65535) return asynError;
    } else {
        if (offset >= pPlc->modbusLength) return asynError;
    }
    return asynSuccess;
}

int checkModbusFunction(PLC_ID pPlc, int *modbusFunction)
{
    /* If this is an initial read operation on a write function code and 
     * pollDelay is not > 0 then return error */
    if (pPlc->readOnceFunction && (pPlc->pollDelay <= 0)) return asynError;
    *modbusFunction = pPlc->readOnceFunction ? pPlc->readOnceFunction : pPlc->modbusFunction;
    return asynSuccess;
}



asynStatus readPlcInt(modbusStr_t *pPlc, modbusDataType_t dataType, int offset, epicsInt32 *output, int *bufferLen)
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
    
    value = pPlc->data[offset];
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
            int16_32.uint16[littleWord] = pPlc->data[offset];
            int16_32.uint16[bigWord]    = pPlc->data[offset+1];
            result = int16_32.int32;
            *bufferLen = 2;
            break;

        case dataTypeInt32BE:
            int16_32.uint16[bigWord]    = pPlc->data[offset];
            int16_32.uint16[littleWord] = pPlc->data[offset+1];
            result = int16_32.int32;
            *bufferLen = 2;
            break;

        case dataTypeFloat32LE:
        case dataTypeFloat32BE:        
        case dataTypeFloat64LE:
        case dataTypeFloat64BE:        
            status = readPlcFloat(pPlc, dataType, offset, &fValue, bufferLen);
            result = (epicsInt32)fValue;
            break;

        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::readPlcInt, port %s unknown data type %d\n", 
                      driver, pPlc->portName, dataType);
            status = asynError;
    }
    *output = result;
    return status;
}


asynStatus writePlcInt(modbusStr_t *pPlc, modbusDataType_t dataType, int offset, epicsInt32 value, epicsUInt16 *buffer, int *bufferLen)
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
            status = writePlcFloat(pPlc, dataType, offset, (epicsFloat64)value, buffer, bufferLen);
            break;

        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::writePlcInt, port %s unknown data type %d\n", 
                      driver, pPlc->portName, dataType);
            status = asynError;
    }

    return status;
}

asynStatus readPlcFloat(modbusStr_t *pPlc, modbusDataType_t dataType, int offset, epicsFloat64 *output, int *bufferLen)
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
    
    switch (dataType) {
        case dataTypeUInt16:
        case dataTypeInt16SM:
        case dataTypeBCDSigned:
        case dataTypeBCDUnsigned:
        case dataTypeInt16:
        case dataTypeInt32LE:
        case dataTypeInt32BE:
            status = readPlcInt(pPlc, dataType, offset, &iValue, bufferLen);
            *output = (epicsFloat64)iValue;
            break;
            
        case dataTypeFloat32LE:
            uIntFloat.ui16[w32_0] = pPlc->data[offset];
            uIntFloat.ui16[w32_1] = pPlc->data[offset+1];
            *output = (epicsFloat64)uIntFloat.f32;
            *bufferLen = 2;
            break;
            
        case dataTypeFloat32BE:
            uIntFloat.ui16[w32_1] = pPlc->data[offset];
            uIntFloat.ui16[w32_0] = pPlc->data[offset+1];
            *output = (epicsFloat64)uIntFloat.f32;
            *bufferLen = 2;
            break;
            
        case dataTypeFloat64LE:
            uIntFloat.ui16[w64_0] = pPlc->data[offset];
            uIntFloat.ui16[w64_1] = pPlc->data[offset+1];
            uIntFloat.ui16[w64_2] = pPlc->data[offset+2];
            uIntFloat.ui16[w64_3] = pPlc->data[offset+3];
            *output = (epicsFloat64)uIntFloat.f64;
            *bufferLen = 4;
            break;
            
        case dataTypeFloat64BE:
            uIntFloat.ui16[w64_3] = pPlc->data[offset];
            uIntFloat.ui16[w64_2] = pPlc->data[offset+1];
            uIntFloat.ui16[w64_1] = pPlc->data[offset+2];
            uIntFloat.ui16[w64_0] = pPlc->data[offset+3];
            *output = (epicsFloat64)uIntFloat.f64;
            *bufferLen = 4;
            break;
            
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::readPlcFloat, port %s unknown data type %d\n", 
                      driver, pPlc->portName, dataType);
            status = asynError;
    }
    return status;
}


asynStatus writePlcFloat(modbusStr_t *pPlc, modbusDataType_t dataType, int offset, epicsFloat64 value, epicsUInt16 *buffer, int *bufferLen)
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
    
    switch (dataType) {
        case dataTypeUInt16:
        case dataTypeInt16SM:
        case dataTypeBCDSigned:
        case dataTypeBCDUnsigned:
        case dataTypeInt16:
        case dataTypeInt32LE:
        case dataTypeInt32BE:
            status = writePlcInt(pPlc, dataType, offset, (epicsInt32)value, buffer, bufferLen);
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
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::writePlcFloat, port %s unsupported data type %d\n", 
                      driver, pPlc->portName, dataType);
            status = asynError;
    }
    return status;
}

static asynStatus readPlcString(modbusStr_t *pPlc, modbusDataType_t dataType, int offset, 
                                char *data, size_t maxChars, int *bufferLen)
{
    int i;
    asynStatus status = asynSuccess;

    for (i=0; i<maxChars && offset<pPlc->modbusLength; i++, offset++) {
        switch (dataType) {
            case dataTypeStringHigh:
                data[i] = (pPlc->data[offset] >> 8) & 0x00ff;
                break;

            case dataTypeStringLow:
                data[i] = pPlc->data[offset] & 0x00ff;
                break;

            case dataTypeStringHighLow:
                data[i] = (pPlc->data[offset] >> 8) & 0x00ff;
                if (i<maxChars-1) {
                    i++;
                    data[i] = pPlc->data[offset] & 0x00ff;
                }
                break;

            case dataTypeStringLowHigh:
                data[i] = pPlc->data[offset] & 0x00ff;
                if (i<maxChars-1) {
                    i++;
                    data[i] = (pPlc->data[offset] >> 8) & 0x00ff;
                }
                break;

            default:
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::readPlcFloat, port %s unknown data type %d\n", 
                          driver, pPlc->portName, dataType);
                status = asynError;
        }
    }
    /* Nil terminate and set number of characters to include trailing nil */
    if (i >= maxChars) {
        i = maxChars-1;
    } 
    data[i] = 0;
    *bufferLen = strlen(data) + 1;
    return status;
}

static asynStatus writePlcString(modbusStr_t *pPlc, modbusDataType_t dataType, int offset, 
                                 const char *data, size_t maxChars, size_t *nActual, int *bufferLen)
{
    int i;
    asynStatus status = asynSuccess;

    for (i=0, *bufferLen=0, *nActual=0; i<maxChars && offset<pPlc->modbusLength; i++, offset++) {
        switch (dataType) {
            case dataTypeStringHigh:
                pPlc->data[offset] = (data[i] << 8) & 0xff00;
                break;

            case dataTypeStringLow:
                pPlc->data[offset] = data[i] & 0x00ff;
                break;

            case dataTypeStringHighLow:
                pPlc->data[offset] = (data[i] << 8) & 0xff00;
                if (i<maxChars-1) {
                    i++;
                    pPlc->data[offset] |= data[i] & 0x00ff;
                }
                break;

            case dataTypeStringLowHigh:
                pPlc->data[offset] = data[i] & 0x00ff;
                if (i<maxChars-1) {
                    i++;
                    pPlc->data[offset] |= (data[i] << 8) & 0xff00;
                }
                break;

            default:
                asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                          "%s::readPlcFloat, port %s unknown data type %d\n", 
                          driver, pPlc->portName, dataType);
                status = asynError;
        }
        *nActual = i + 1;
        (*bufferLen)++;
    }
    return status;
}


/* iocsh functions */

static const iocshArg ConfigureArg0 = {"Port name",            iocshArgString};
static const iocshArg ConfigureArg1 = {"Octet port name",      iocshArgString};
static const iocshArg ConfigureArg2 = {"Modbus slave address", iocshArgInt};
static const iocshArg ConfigureArg3 = {"Modbus function code", iocshArgInt};
static const iocshArg ConfigureArg4 = {"Modbus start address", iocshArgInt};
static const iocshArg ConfigureArg5 = {"Modbus length",        iocshArgInt};
static const iocshArg ConfigureArg6 = {"Data type (0=binary, 1=BCD)", iocshArgInt};
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
