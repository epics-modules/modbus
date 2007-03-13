/*----------------------------------------------------------------------**
*  file:        drvModbusTCPAsyn.c                                      **
*-----------------------------------------------------------------------**
* EPICS asyn driver support for Modbus protocol communication with PLCs **
* over TCP/IP.                                                          **
* 
* Mark Rivers, University of Chicago
* Original Date March 3, 2007
*
* Based on the modtcp and plctcp code from Rolf Keitel of Triumf, with  **
* work from Ivan So at NSLS.
*-----------------------------------------------------------------------**
*
*/


/* ANSI C includes  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* EPICS includes */
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <cantProceed.h>
#include <errlog.h>
#include <osiSock.h>
#include <iocsh.h>
#include <epicsExport.h>

/* Asyn includes */
#include "asynDriver.h"
#include "asynDrvUser.h"
#include "asynOctetSyncIO.h"
#include "asynInt32.h"
#include "asynUInt32Digital.h"
#include "asynInt32Array.h"
#include "asynFloat64.h"

#include "modbusTCP.h"
#include "drvModbusTCPAsyn.h"

/* defined constants */

#define PLC_MODICON 0
#define PLC_SIEMENS 1

#define MAX_READ_WORDS  125        /* Modbus limit on number of words to read */
#define TIMEHISTLENGTH 200         /* length of time histogram */
#define MAX_TCP_MESSAGE_SIZE 1500
#define MAX_CONNECT_RETRY  5
#define DEFAULT_DELAY 3
#define DEFAULT_RECONNECT 1
#define MODBUS_READ_TIMEOUT 2.0


/* typedefs  */
typedef enum {
    modbusDataCommand,
    modbusStartHistogramCommand,
    modbusStopHistogramCommand,
    modbusReadHistogramCommand
} modbusCommand;

#define MAX_MODBUS_COMMANDS 4

typedef struct {
    modbusCommand command;
    char *commandString;
} modbusCommandStruct;

static modbusCommandStruct modbusCommands[MAX_MODBUS_COMMANDS] = {
    {modbusDataCommand,            MODBUS_DATA_COMMAND_STRING},            /* all, read/write */
    {modbusStartHistogramCommand,  MODBUS_START_HISTOGRAM_COMMAND_STRING}, /* int32, write */
    {modbusStopHistogramCommand,   MODBUS_STOP_HISTOGRAM_COMMAND_STRING},  /* int32, write */
    {modbusReadHistogramCommand,   MODBUS_READ_HISTOGRAM_COMMAND_STRING},  /* int32Array, read */
};

typedef struct modbusTCPStr *PLC_ID;

typedef struct modbusTCPStr
{
    PLC_ID pNext;            /* Pointer to next asyn port server */
    char *portName;         /* asyn port name for this server */
    char *tcpPortName;      /* asyn port name for the asyn TCP port */
    char *plcType;
    asynUser  *pasynUserOctet;
    asynInterface asynCommon;
    asynInterface asynDrvUser;
    asynInterface asynUint32D;
    asynInterface asynInt32;
    asynInterface asynInt32Array;
    asynInterface asynFloat64;
    asynUser   *pasynUserTrace;
    void *asynUint32DInterruptPvt;
    void *asynInt32InterruptPvt;
    int modbusFunction;
    int modbusStartAddress;
    int modbusLength;            /* Number of words or bits of Modbus data */
    unsigned short *data;        /* Memory buffer */
    unsigned short *prevData;    /* Previous contents of memory buffer */
    char modbusRequest[MAX_TCP_MESSAGE_SIZE];      /* Modbus request message */
    char modbusReply[MAX_TCP_MESSAGE_SIZE];        /* Modbus reply message */
    double pollTime;
    int readOnceFunction;
    int readOnceDone;
    epicsThreadId readTaskId;        /* id of read task */
    int checkTaskId;       /* id of watchdog check task */
    int abort;
    int readOK;
    int writeOK;
    int readBad;
    int writeBad;
    int maxReadTime;
    int last_readok;
    int noupd_count;
    int read_retry;
    int write_retry;     
    int auto_reconnect;
    long plcwatch_disconn; /* if >0, then disconnect if no reads for some time */
    long max_noupd_count;     /* maximum noread intervals (5 sec) before disconnecting */
    unsigned long readmsec; 
    long *timeHist;        /* histogram of read-times */
    long *groupTimes;     /* histogram of group read times */
    int timeLong;
    int enbHist;
    int watchdogOffset;
    int keepaliveOffset;
    int watchdogLast;
    int watchdogOk;
} modbusTCPStr_t;


/* local variable declarations */
static PLC_ID pFirstPlc;
static PLC_ID diagPlc;

static char *driver = "drvPlctcp";         /* id for asynPrint */

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
static asynStatus setInterrupt     (void *drvPvt, asynUser *pasynUser,
                                    epicsUInt32 mask, interruptReason reason);
static asynStatus clearInterrupt   (void *drvPvt, asynUser *pasynUser,
                                    epicsUInt32 mask);
static asynStatus getInterrupt     (void *drvPvt, asynUser *pasynUser,
                                    epicsUInt32 *mask, interruptReason reason);

/* These functions are in the asynInt32 interface */
static asynStatus writeInt32       (void *drvPvt, asynUser *pasynUser,
                                    epicsInt32 value);
static asynStatus readInt32        (void *drvPvt, asynUser *pasynUser,
                                    epicsInt32 *value);
static asynStatus getBounds        (void *drvPvt, asynUser *pasynUser,
                                    epicsInt32 *low, epicsInt32 *high);

static asynStatus writeFloat64     (void *drvPvt, asynUser *pasynUser,
                                    epicsFloat64 value);
static asynStatus readFloat64      (void *drvPvt, asynUser *pasynUser,
                                    epicsFloat64 *value);

static asynStatus readInt32Array   (void *drvPvt, asynUser *pasynUser,
                                    epicsInt32 *data, size_t maxChans,
                                    size_t *nactual);
static asynStatus writeInt32Array  (void *drvPvt, asynUser *pasynUser,
                                    epicsInt32 *data, size_t maxChans);

static void plcRead(PLC_ID pPlc);
static int doModbusIO(PLC_ID pPlc, int function, int start, unsigned short *data, int len);


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




/********************************************************************
**  global driver functions
*********************************************************************
*/

/*
** drvModbusTCPAsynConfigure() - create and init an asyn port driver for a PLC
**                                                                    
** SYNOPSIS    int plctcpDrvConfigure                                        
**                {                                                     
**                char *portName      asyn port name for this server              
**                char *tcpPort       asyn port name for TCP server
**                char *plcType       "Modicon", "Siemens", "Wago", "Koyu"
**                int  modbusFunction Modbus function code
**                int  modbusStartAddress Starting address on Modbus
**                int  modbusLength   Number of Modbus registers
**                }                                                    
**                                                                    
** RETURNS    0 (success) or -1 (error)
**
** this function is called from IOC startup script: once for each register set                                                                   
*/

int drvModbusTCPAsynConfigure(char *portName, char *tcpPortName, char *plcType,
                           int modbusFunction, int modbusStartAddress, int modbusLength,
                           int pollMsec)
{
    int status;
    PLC_ID pPlc;
    char readThreadName[100];
    int needReadThread;
    int readLength;

    pPlc = callocMustSucceed(1, sizeof(*pPlc), "drvModbusTCPAsynConfigure");
    pPlc->portName = epicsStrDup(portName);
    pPlc->tcpPortName = epicsStrDup(tcpPortName);
    pPlc->plcType = epicsStrDup(plcType);
    pPlc->modbusFunction = modbusFunction;
    pPlc->modbusStartAddress = modbusStartAddress;
    pPlc->modbusLength = modbusLength;
    pPlc->pollTime = pollMsec/1000.;
    
    /* Connect to TCP asyn port with asynOctetSyncIO */
    status = pasynOctetSyncIO->connect(tcpPortName, 0, &pPlc->pasynUserOctet, 0);
    if (status != asynSuccess) {
        errlogPrintf("%s::drvModbusTCPAsynConfigure %s ERROR: Can't connect to TCP server %s.\n",
                     driver, portName, tcpPortName);
        return -1;
    }

    /* Create asyn interfaces and register with asynManager */
    pPlc->asynCommon.interfaceType = asynCommonType;
    pPlc->asynCommon.pinterface  = (void *)&drvCommon;
    pPlc->asynCommon.drvPvt = pPlc;
    pPlc->asynDrvUser.interfaceType = asynDrvUserType;
    pPlc->asynDrvUser.pinterface  = (void *)&drvUser;
    pPlc->asynDrvUser.drvPvt = pPlc;
    pPlc->asynUint32D.interfaceType = asynUInt32DigitalType;
    pPlc->asynUint32D.pinterface  = (void *)&drvUInt32D;
    pPlc->asynUint32D.drvPvt = pPlc;
    pPlc->asynInt32.interfaceType = asynInt32Type;
    pPlc->asynInt32.pinterface  = (void *)&drvInt32;
    pPlc->asynInt32.drvPvt = pPlc;
    pPlc->asynFloat64.interfaceType = asynFloat64Type;
    pPlc->asynFloat64.pinterface  = (void *)&drvFloat64;
    pPlc->asynFloat64.drvPvt = pPlc;
    pPlc->asynInt32Array.interfaceType = asynInt32ArrayType;
    pPlc->asynInt32Array.pinterface  = (void *)&drvInt32Array;
    pPlc->asynInt32Array.drvPvt = pPlc;
    /* Can block needs to be set based on function code */
    status = pasynManager->registerPort(pPlc->portName,
                                        1, /* multiDevice, can't block */
                                        1, /* autoconnect */
                                        0, /* medium priority */
                                        0); /* default stack size */
    if (status != asynSuccess) {
        errlogPrintf("drvModbusTCPAsynConfigure ERROR: Can't register port\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPlc->portName,&pPlc->asynCommon);
    if (status != asynSuccess) {
        errlogPrintf("drvModbusTCPConfigure ERROR: Can't register common.\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPlc->portName,&pPlc->asynDrvUser);
    if (status != asynSuccess) {
        errlogPrintf("drvModbusTCPConfigure ERROR: Can't register drvUser.\n");
        return -1;
    }
    status = pasynUInt32DigitalBase->initialize(pPlc->portName, &pPlc->asynUint32D);
    if (status != asynSuccess) {
        errlogPrintf("drvModbusTCPConnfigure ERROR: Can't register UInt32Digital.\n");
        return -1;
    }
    pasynManager->registerInterruptSource(pPlc->portName, &pPlc->asynUint32D,
                                          &pPlc->asynUint32DInterruptPvt);
    status = pasynInt32Base->initialize(pPlc->portName,&pPlc->asynInt32);
    if (status != asynSuccess) {
        errlogPrintf("drvModbusTCPConfigure ERROR: Can't register Int32.\n");
        return -1;
    }
    pasynManager->registerInterruptSource(pPlc->portName, &pPlc->asynInt32,
                                          &pPlc->asynInt32InterruptPvt);
    status = pasynManager->registerInterface(pPlc->portName,&pPlc->asynFloat64);
    if (status != asynSuccess) {
        errlogPrintf("drvModbusTCPConfigure ERROR: Can't register Float64.\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPlc->portName,&pPlc->asynInt32Array);
    if (status != asynSuccess) {
        errlogPrintf("drvModbusTCPConfigure ERROR: Can't register Int32Array.\n");
        return -1;
    }

    /* Create asynUser for asynTrace */
    pPlc->pasynUserTrace = pasynManager->createAsynUser(0, 0);
    pPlc->pasynUserTrace->userPvt = pPlc;

    /* Connect to device */
    status = pasynManager->connectDevice(pPlc->pasynUserTrace, pPlc->portName, 0);
    if (status != asynSuccess) {
        errlogPrintf("drvModbusTCPConfigure, connectDevice failed %s\n",
                     pPlc->pasynUserTrace->errorMessage);
        return(-1);
    }
    
    readLength = 0;
    needReadThread = 0;
    pPlc->readOnceFunction = 0;
   switch(pPlc->modbusFunction) {
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
            readLength = pPlc->modbusLength/16;
            needReadThread = 1;
            break;
        case MODBUS_READ_HOLDING_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS:
            readLength = pPlc->modbusLength;
            needReadThread = 1;
            break;
        case MODBUS_WRITE_SINGLE_COIL:
        case MODBUS_WRITE_MULTIPLE_COILS:
            readLength = pPlc->modbusLength/16;
            if (pollMsec != 0) pPlc->readOnceFunction = MODBUS_READ_COILS;
            break;
       case MODBUS_WRITE_SINGLE_REGISTER:
       case MODBUS_WRITE_MULTIPLE_REGISTERS:
            readLength = pPlc->modbusLength;
            if (pollMsec != 0) pPlc->readOnceFunction = MODBUS_READ_HOLDING_REGISTERS;
            break;
       default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::drvModbusTCPAsynConfid PLC %s unsupported Modbus function %d\n",
                      driver, pPlc->portName, pPlc->modbusFunction);
            return(asynError);
    }
    
    /* Make sure memory length is valid for this modbusFunction FIX THIS */
    if (readLength > MAX_READ_WORDS) {
        errlogPrintf("drvModbusTCPConfigure, memory length too large\n");
        return(-1);
    }
    
    /* Note that we always allocate modbusLength words of memory.  In theory we could use modbusLength bits for bit
     * operations, doModbusIO puts one bit per word for simplicity.  Also write operations without readOnce do not
     * need the memory, but it is at most 250 bytes, so we don't worry about it */
    if (pPlc->modbusLength != 0) {
        pPlc->data = callocMustSucceed(pPlc->modbusLength, sizeof(unsigned short), "drvModbusTCPAsynConfigure");
        pPlc->prevData = callocMustSucceed(pPlc->modbusLength, sizeof(unsigned short), "drvModbusTCPAsynConfigure");
    }
    pPlc->pNext = NULL;
    pPlc->plcwatch_disconn = 1;    /* default: watchdog task disconnects */
    pPlc->max_noupd_count = 2;     /* ... after 10 seconds of no communication */
    pPlc->auto_reconnect = DEFAULT_RECONNECT;
 
    /* If this is an output function do a readOnce operation if required */
    if (pPlc->readOnceFunction) {
        status = doModbusIO(pPlc, pPlc->readOnceFunction, pPlc->modbusStartAddress, 
                            pPlc->data, pPlc->modbusLength);
        if (status != asynSuccess) {
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::drvModbusTCPAsynConfigure PLC %s error=%d performing readOnce operation %d\n",
                      driver, pPlc->portName, status, pPlc->readOnceFunction);
            return(asynError);
        }
        pPlc->readOnceDone = 1;
    }
    
    /* Create the thread to read registers if this is a read function code */
    if (needReadThread) {
        epicsSnprintf(readThreadName, 100, "%sRead", pPlc->portName);
        pPlc->readTaskId = epicsThreadCreate(readThreadName,
           epicsThreadPriorityMedium,
           epicsThreadGetStackSize(epicsThreadStackSmall),
           (EPICSTHREADFUNC)plcRead, 
           pPlc);
    }

    return 0;
}


/* asynDrvUser routines */
static asynStatus drvUserCreate(void *drvPvt, asynUser *pasynUser,
                                const char *drvInfo,
                                const char **pptypeName, size_t *psize)
{
    int i;
    char *pstring;

    for (i=0; i<MAX_MODBUS_COMMANDS; i++) {
        pstring = modbusCommands[i].commandString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynUser->reason = modbusCommands[i].command;
            if (pptypeName) *pptypeName = epicsStrDup(pstring);
            if (psize) *psize = sizeof(modbusCommands[i].command);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                      "drvModbusTCPAsyn::drvUserConfigure, command=%s\n", pstring);
            return(asynSuccess);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvModbusTCPAsyn::drvUserConfigure, unknown command=%s\n", drvInfo);
    return(asynError);
}

static asynStatus drvUserGetType(void *drvPvt, asynUser *pasynUser,
                                 const char **pptypeName, size_t *psize)
{
    int command = pasynUser->reason;

    *pptypeName = NULL;
    *psize = 0;
    if (pptypeName)
        *pptypeName = epicsStrDup(modbusCommands[command].commandString);
    if (psize) *psize = sizeof(command);
    return(asynSuccess);
}

static asynStatus drvUserDestroy(void *drvPvt, asynUser *pasynUser)
{
    return(asynSuccess);
}


/***********************/
/* asynCommon routines */
/***********************/

/* Connect */
static asynStatus asynConnect(void *drvPvt, asynUser *pasynUser)
{
    PLC_ID pPvt = (PLC_ID)drvPvt;
    int signal;
  
    pasynManager->getAddr(pasynUser, &signal);
    if (signal < pPvt->modbusLength) {
        pasynManager->exceptionConnect(pasynUser);
        return(asynSuccess);
    } else {
        return(asynError);
    }
}

/* Disconnect */
static asynStatus asynDisconnect(void *drvPvt, asynUser *pasynUser)
{
    /* Does nothing for now.  
     * May be used if connection management is implemented */
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}


/* Report  parameters */
static void asynReport(void *drvPvt, FILE *fp, int details)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;

    fprintf(fp, "modbusTCP port: %s\n", pPlc->portName);
    if (details) {
        fprintf(fp, "    asyn TCP server:    %s\n", pPlc->tcpPortName);
        fprintf(fp, "    modbusFunction:     %d\n", pPlc->modbusFunction);
        fprintf(fp, "    modbusStartAddress: %o\n", pPlc->modbusStartAddress);
        fprintf(fp, "    modbusLength:       %o\n", pPlc->modbusLength);
        fprintf(fp, "    plcType:            %s\n", pPlc->plcType);
        fprintf(fp, "    read:               ok %d bad %d\n", pPlc->readOK, pPlc->readBad);
        fprintf(fp, "    write:              ok %d bad %d\n", pPlc->writeOK, pPlc->writeBad);
        fprintf(fp, "    pollTime:           %f\n", pPlc->pollTime);
        fprintf(fp, "    time for last read: %ld msec\n", pPlc->readmsec);
        fprintf(fp, "    Max. read time:     %d msec\n", pPlc->maxReadTime);
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
    

    if (pasynUser->reason != modbusDataCommand) {
        asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                  "%s::readUInt32D PLC %s invalid pasynUser->reason %d\n",
                  driver, pPlc->portName, pasynUser->reason);
        return(asynError);
    }
    
    pasynManager->getAddr(pasynUser, &offset);

    *value = 0;
    if (offset >= pPlc->modbusLength) {
        asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                  "%s::readUInt32D PLC %s invalid memory request %d, max=%d\n",
                  driver, pPlc->portName, offset, pPlc->modbusLength);
        return(asynError);
    }

    switch(pPlc->modbusFunction) {
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
             *value = pPlc->data[offset];
             break;
         case MODBUS_WRITE_SINGLE_COIL:
         case MODBUS_WRITE_MULTIPLE_COILS:
             if (!pPlc->readOnceDone) return(asynError);
             *value = pPlc->data[offset];
             break;
        case MODBUS_READ_HOLDING_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS:
            *value = pPlc->data[offset];
            if ((mask != 0 ) && (mask != 0xFFFF)) *value &= mask;
            break;
        case MODBUS_WRITE_SINGLE_REGISTER:
        case MODBUS_WRITE_MULTIPLE_REGISTERS:
            if (!pPlc->readOnceDone) return(asynError);
            *value = pPlc->data[offset];
            if ((mask != 0 ) && (mask != 0xFFFF)) *value &= mask;
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::readUInt32D PLC %s invalid request for Modbus function %d\n",
                      driver, pPlc->portName, pPlc->modbusFunction);
            return(asynError);
    }
    
    return(asynSuccess);
}


static asynStatus writeUInt32D(void *drvPvt, asynUser *pasynUser, epicsUInt32 value,
                               epicsUInt32 mask)
{
    PLC_ID pPlc = (PLC_ID)drvPvt;
    int offset;
    int modbusAddress;
    unsigned short reg, data = value;
    asynStatus status;


    if (pasynUser->reason != modbusDataCommand) {
        asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                  "%s::writeUInt32D PLC %s invalid pasynUser->reason %d\n",
                  driver, pPlc->portName, pasynUser->reason);
        return(asynError);
    }
    
    pasynManager->getAddr(pasynUser, &offset);
    modbusAddress = pPlc->modbusStartAddress + offset;

    if (offset >= pPlc->modbusLength) {
        asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                  "%s::writeUInt32D PLC %s invalid memory request %d, max=%d\n",
                  driver, pPlc->portName, offset, pPlc->modbusLength);
        return(asynError);
    }
    
    switch(pPlc->modbusFunction) {
        case MODBUS_WRITE_SINGLE_COIL:
            status = doModbusIO(pPlc, pPlc->modbusFunction, modbusAddress, 
                                &data, 1);
            if (status != asynSuccess) return(status);
            break;
        case MODBUS_WRITE_SINGLE_REGISTER:
            /* Do this as a read/modify/write if mask is not all 0 or all 1 */
            if ((mask == 0) || (mask == 0xFFFF)) {
                status = doModbusIO(pPlc, pPlc->modbusFunction, modbusAddress, 
                                    &data, 1);
            } else {
                status = doModbusIO(pPlc, MODBUS_READ_HOLDING_REGISTERS, modbusAddress, 
                                    &reg, 1);
                if (status != asynSuccess) return(status);
                /* Set bits that are set in the value and set in the mask */
                reg |=  (data & mask);
                /* Clear bits that are clear in the value and set in the mask */
                reg  &= (data | ~mask);
                status = doModbusIO(pPlc, pPlc->modbusFunction, modbusAddress, 
                                    &reg, 1);
            }
            if (status != asynSuccess) return(status);
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::writeUInt32D PLC %s invalid request for Modbus function %d\n",
                      driver, pPlc->portName, pPlc->modbusFunction);
            return(asynError);
    }
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s::writeUInt32D, *value=%x\n", driver, value);
    return(asynSuccess);
}

static asynStatus setInterrupt (void *drvPvt, asynUser *pasynUser, epicsUInt32 mask, 
                                interruptReason reason)
{
    return(asynSuccess);
}
                                
static asynStatus clearInterrupt (void *drvPvt, asynUser *pasynUser, epicsUInt32 mask)
{
    return(asynSuccess);
}

static asynStatus getInterrupt (void *drvPvt, asynUser *pasynUser, epicsUInt32 *mask, 
                                interruptReason reason)
{
    return(asynSuccess);
}
                                


/* 
**  asynInt32 support
*/

static asynStatus writeInt32 (void *drvPvt, asynUser *pasynUser, epicsInt32 value)
{
    return(asynSuccess);
}

static asynStatus readInt32 (void *drvPvt, asynUser *pasynUser, epicsInt32 *value)
{
    return(asynSuccess);
}

static asynStatus getBounds (void *drvPvt, asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high)
{
    return(asynSuccess);
}



/* 
**  asynFloat64 support
*/
static asynStatus writeFloat64 (void *drvPvt, asynUser *pasynUser, epicsFloat64 value)
{
    return(asynSuccess);
}

static asynStatus readFloat64 (void *drvPvt, asynUser *pasynUser, epicsFloat64 *value)
{
    return(asynSuccess);
}


/* 
**  asynInt32Array support
*/
static asynStatus readInt32Array (void *drvPvt, asynUser *pasynUser, epicsInt32 *data, size_t maxChans,
                                  size_t *nactual)
{
    return(asynSuccess);
}

static asynStatus writeInt32Array (void *drvPvt, asynUser *pasynUser, epicsInt32 *data, size_t maxChans)
{
    return(asynSuccess);
}



/*
****************************************************************************
** "task" function for continuous PLC reads
          one instance spawned per asynPort
****************************************************************************
*/

static void plcRead(PLC_ID pPlc)
{

    asynStatus status;

    /* Loop in the background */    
    while (1)
    {
        status = doModbusIO(pPlc, pPlc->modbusFunction, pPlc->modbusStartAddress, 
                            pPlc->data, pPlc->modbusLength);
        epicsThreadSleep(pPlc->pollTime);
    }
}



/********************************************************************
*********************************************************************
**  local driver functions
*********************************************************************
*********************************************************************
*/

static int doModbusIO(PLC_ID pPlc, int function, int start, unsigned short *data, int len)
{
    modbusReadRequest *readReq;
    modbusReadResponse *readResp;
    modbusWriteSingleRequest *writeSingleReq;
    modbusWriteSingleResponse *writeSingleResp;
    modbusWriteMultipleRequest *writeMultipleReq;
    modbusWriteMultipleResponse *writeMultipleResp;
    modbusExceptionResponse *exceptionResp;
    int requestSize=0;
    int replySize=MAX_TCP_MESSAGE_SIZE;
    unsigned short transactId=1;
    unsigned short cmdLength;
    unsigned char  destId=0xFF;
    unsigned short modbusEncoding=0;
    unsigned char  *pCharIn;
    unsigned short *pShortIn, *pShortOut;
    unsigned short bitOutput;
    asynStatus status;
    int i;
    epicsTimeStamp startTime, endTime;
    size_t nwrite, nread;
    int eomReason;
    double dT;
    int msec;
    unsigned char mask;

    /* First build the parts of the message that are independent of the function type */
    readReq = (modbusReadRequest *)pPlc->modbusRequest;
    readReq->mbapHeader.transactId    = htons(transactId);
    readReq->mbapHeader.protocolType  = htons(modbusEncoding);
    readReq->mbapHeader.destId        = destId;

   switch (function) {
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
        case MODBUS_READ_HOLDING_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS:
            readReq = (modbusReadRequest *)pPlc->modbusRequest;
            cmdLength = sizeof(modbusReadRequest) - sizeof(modbusMBAPHeader) + 2;
            readReq->mbapHeader.cmdLength = htons(cmdLength);
            readReq->fcode = function;
            readReq->startReg = htons((unsigned short)start);
            readReq->numRead = htons((unsigned short)len);
            requestSize = sizeof(modbusReadRequest);
            break;
        case MODBUS_WRITE_SINGLE_COIL:
            writeSingleReq = (modbusWriteSingleRequest *)pPlc->modbusRequest;
            cmdLength = sizeof(modbusWriteSingleRequest) - sizeof(modbusMBAPHeader) + 2;
            writeSingleReq->mbapHeader.cmdLength = htons(cmdLength);
            writeSingleReq->fcode = function;
            writeSingleReq->startReg = htons((unsigned short)start);
            if (*data) bitOutput = 0xFF00;
            else       bitOutput = 0;
            writeSingleReq->data = htons(bitOutput);
            requestSize = sizeof(modbusWriteSingleRequest);
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                      "%s::doModbusIO PLC %s WRITE_SINGLE_COIL address=%o value=%x\n",
                      driver, pPlc->portName, start, bitOutput);
            break;
        case MODBUS_WRITE_SINGLE_REGISTER:
            writeSingleReq = (modbusWriteSingleRequest *)pPlc->modbusRequest;
            cmdLength = sizeof(modbusWriteSingleRequest) - sizeof(modbusMBAPHeader) + 2;
            writeSingleReq->mbapHeader.cmdLength = htons(cmdLength);
            writeSingleReq->fcode = function;
            writeSingleReq->startReg = htons((unsigned short)start);
            writeSingleReq->data = htons((unsigned short)*data);
            requestSize = sizeof(modbusWriteSingleRequest);
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                      "%s::doModbusIO PLC %s WRITE_SINGLE_REGISTER address=%o value=%x\n",
                      driver, pPlc->portName, start, data);
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR, 
                      "%s::doModbusIO, unsupported function code %d\n", 
                      driver, function);
            break;
    }

    /* First we do connection stuff */
        
    /* give up the connection attempts after MAX_CONNECT_RETRY retries */
    if (pPlc->read_retry > MAX_CONNECT_RETRY || pPlc->write_retry > MAX_CONNECT_RETRY ) {
        asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR, 
                 "\n%s: *** %s read socket - too many reconnect attempts\n", 
                 driver, pPlc->portName);
    }

     /* See if we are connected with pasynManager->isConnected */

     /* If not connected then called asynCommon->connect (or connectDevice?) */

     pPlc->read_retry = 0;


    /* Do the Modbus I/O as a write/read cycle */
    epicsTimeGetCurrent(&startTime);
    status = pasynOctetSyncIO->writeRead(pPlc->pasynUserOctet, 
                                         pPlc->modbusRequest, requestSize,
                                         pPlc->modbusReply, replySize,
                                         MODBUS_READ_TIMEOUT,
                                         &nwrite, &nread, &eomReason);
    epicsTimeGetCurrent(&endTime);
                                         
    if (status != asynSuccess) {
        asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                 "%s::doModbusIO: %s error calling writeRead, error=%s, nwrite=%d/%d, nread=%d\n", 
                 driver, pPlc->portName, pPlc->pasynUserOctet->errorMessage, nwrite, requestSize, nread);
        pPlc->readBad++;
        return (asynError);
    }
               
    pPlc->readOK++;
    dT = epicsTimeDiffInSeconds(&endTime, &startTime);
    msec = dT*1000;
    pPlc->readmsec = msec;
    if (msec > pPlc->maxReadTime) pPlc->maxReadTime = msec;
    if (pPlc->enbHist) {
         if (msec >= TIMEHISTLENGTH) pPlc->timeLong++;
        else if (msec >= 0) {
            pPlc->timeHist[msec] = (pPlc->timeHist[msec] > 1000000 ? 0 : pPlc->timeHist[msec] + 1.);
        }
    }

    /* See if there is a Modbus exception */
    readResp = (modbusReadResponse *)pPlc->modbusReply;
    if (readResp->fcode & MODBUS_EXCEPTION_FCN) {
        exceptionResp = (modbusExceptionResponse *)pPlc->modbusReply;
        asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                  "%s::doModbusIO: %s Modbus exception=%d\n", 
                  driver, pPlc->portName, exceptionResp->exception);
        return(asynError);
    }

    /* Make sure the function code in the response is the same as the one in the request? */

    switch (function) {
        /* Need to handle all function codes here FIX THIS */
        case MODBUS_READ_COILS:
        case MODBUS_READ_DISCRETE_INPUTS:
            readResp = (modbusReadResponse *)pPlc->modbusReply;
            nread = readResp->byteCount;
            pCharIn = (unsigned char *)&readResp->data;
            pShortOut = (unsigned short *)data;
            mask = 0x01;
            /* Subtract 1 because it will be incremented first time */
            pCharIn--;
            /* We assume we got len bits back, since we are only told bytes */
            for (i=0; i<len; i++) {
                if (i%8 == 0) {
                    mask = 0x01;
                    pCharIn++;
                }
                *pShortOut++ = (*pCharIn & mask) ? 1:0;
                mask = mask << 1;
            }
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)data, len*2, 
                        "%s::doModbusIO PLC %s READ_COILS\n",
                        driver, pPlc->portName);
            break;
        case MODBUS_READ_HOLDING_REGISTERS:
        case MODBUS_READ_INPUT_REGISTERS:
            readResp = (modbusReadResponse *)pPlc->modbusReply;
            nread = readResp->byteCount/2;
            pShortIn = (unsigned short *)&readResp->data;
            pShortOut = (unsigned short *)data;
            for (i=0; i<nread; i++) { 
                *pShortOut++ = ntohs(*pShortIn++);
            }
            asynPrintIO(pPlc->pasynUserTrace, ASYN_TRACEIO_DRIVER, 
                        (char *)data, nread, 
                        "%s::doModbusIO PLC %s READ_REGISTERS",
                        driver, pPlc->portName);
            break;
        case MODBUS_WRITE_SINGLE_COIL:
        case MODBUS_WRITE_SINGLE_REGISTER:
            /* Nothing to do */
            break;
        default:
            asynPrint(pPlc->pasynUserTrace, ASYN_TRACE_ERROR,
                      "%s::doModbusIO, unsupported function code %d\n", 
                      driver, function);
            break;
    }

    return asynSuccess;
}




static const iocshArg ConfigureArg0 = {"Port name",            iocshArgString};
static const iocshArg ConfigureArg1 = {"TCP/IP port name",     iocshArgString};
static const iocshArg ConfigureArg2 = {"PLC type",             iocshArgString};
static const iocshArg ConfigureArg3 = {"Modbus function code", iocshArgInt};
static const iocshArg ConfigureArg4 = {"Modbus start address", iocshArgInt};
static const iocshArg ConfigureArg5 = {"Modbus length",        iocshArgInt};
static const iocshArg ConfigureArg6 = {"Poll time (msec)",     iocshArgInt};

static const iocshArg * const drvModbusTCPAsynConfigureArgs[7] = {
	&ConfigureArg0,
	&ConfigureArg1,
	&ConfigureArg2,
	&ConfigureArg3,
	&ConfigureArg4,
	&ConfigureArg5,
        &ConfigureArg6
};

static const iocshFuncDef drvModbusTCPAsynConfigureFuncDef={"drvModbusTCPAsynConfigure", 7,
                                                            drvModbusTCPAsynConfigureArgs};
static void drvModbusTCPAsynConfigureCallFunc(const iocshArgBuf *args)
{
  drvModbusTCPAsynConfigure(args[0].sval, args[1].sval, args[2].sval, args[3].ival, 
                            args[4].ival, args[5].ival, args[6].ival);
}


static void drvModbusTCPAsynRegister(void)
{
  iocshRegister(&drvModbusTCPAsynConfigureFuncDef,drvModbusTCPAsynConfigureCallFunc);
}

epicsExportRegistrar(drvModbusTCPAsynRegister);
