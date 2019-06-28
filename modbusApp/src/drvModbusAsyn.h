/* drvModbusAsyn.h
 *
 *   Author: Mark Rivers
 *
 *   These are the public definitions for drvModbusAsyn.
 * 
 */

#ifndef drvModbusAsyn_H
#define drvModbusAsyn_H

#include <epicsThread.h>
#include <epicsEvent.h>

#include <asynPortDriver.h>
#include "modbus.h"

/* These are the strings that device support passes to drivers via 
 * the asynDrvUser interface.
 * Drivers must return a value in pasynUser->reason that is unique 
 * for that command.
 */

// These are the parameters we register with asynPortDriver
#define MODBUS_DATA_STRING                "MODBUS_DATA" 
#define MODBUS_READ_STRING                "MODBUS_READ" 
#define MODBUS_ENABLE_HISTOGRAM_STRING    "ENABLE_HISTOGRAM"
#define MODBUS_READ_HISTOGRAM_STRING      "READ_HISTOGRAM"
#define MODBUS_HISTOGRAM_BIN_TIME_STRING  "HISTOGRAM_BIN_TIME"
#define MODBUS_HISTOGRAM_TIME_AXIS_STRING "HISTOGRAM_TIME_AXIS"
#define MODBUS_POLL_DELAY_STRING          "POLL_DELAY"
#define MODBUS_READ_OK_STRING             "READ_OK"
#define MODBUS_WRITE_OK_STRING            "WRITE_OK"
#define MODBUS_IO_ERRORS_STRING           "IO_ERRORS"
#define MODBUS_LAST_IO_TIME_STRING        "LAST_IO_TIME"
#define MODBUS_MAX_IO_TIME_STRING         "MAX_IO_TIME"

// These are the data type strings that are used in the drvUser parameter
// They are not registered with asynPortDriver
#define MODBUS_UINT16_STRING           "UINT16" 
#define MODBUS_INT16_SM_STRING         "INT16SM" 
#define MODBUS_BCD_UNSIGNED_STRING     "BCD_UNSIGNED" 
#define MODBUS_BCD_SIGNED_STRING       "BCD_SIGNED" 
#define MODBUS_INT16_STRING            "INT16" 
#define MODBUS_INT32_LE_STRING         "INT32_LE" 
#define MODBUS_INT32_BE_STRING         "INT32_BE" 
#define MODBUS_FLOAT32_LE_STRING       "FLOAT32_LE" 
#define MODBUS_FLOAT32_BE_STRING       "FLOAT32_BE" 
#define MODBUS_FLOAT64_LE_STRING       "FLOAT64_LE" 
#define MODBUS_FLOAT64_BE_STRING       "FLOAT64_BE" 
#define MODBUS_STRING_HIGH_STRING      "STRING_HIGH" 
#define MODBUS_STRING_LOW_STRING       "STRING_LOW" 
#define MODBUS_STRING_HIGH_LOW_STRING  "STRING_HIGH_LOW" 
#define MODBUS_STRING_LOW_HIGH_STRING  "STRING_LOW_HIGH" 

#define HISTOGRAM_LENGTH 200  /* Length of time histogram */

typedef enum {
    dataTypeUInt16,           /* 16-bit unsigned               drvUser=UINT16 */
    dataTypeInt16SM,          /* 16-bit sign and magnitude     drvUser=INT16SM */
    dataTypeBCDUnsigned,      /* 16-bit unsigned BCD           drvUser=BCD_SIGNED */
    dataTypeBCDSigned,        /* 16-bit signed BCD             drvUser=BCD_UNSIGNED */
    dataTypeInt16,            /* 16-bit 2's complement         drvUser=INT16 */
    dataTypeInt32LE,          /* 32-bit integer little-endian  drvUser=INT32_LE */
    dataTypeInt32BE,          /* 32-bit integer big-endian     drvUser=INT32_BE */
    dataTypeFloat32LE,        /* 32-bit float little-endian    drvuser=FLOAT32_LE */
    dataTypeFloat32BE,        /* 32-bit float big-endian       drvUser=FLOAT32_BE */
    dataTypeFloat64LE,        /* 64-bit float little-endian    drvuser=FLOAT64_LE */
    dataTypeFloat64BE,        /* 64-bit float big-endian       drvUser=FLOAT64_BE */
    dataTypeStringHigh,       /* String, high byte of each word           drvUser=STRING_HIGH */
    dataTypeStringLow,        /* String, low byte of each word            drvUser=STRING_LOW*/
    dataTypeStringHighLow,    /* String, high then low byte of each word  drvUser=STRING_HIGH_LOW */
    dataTypeStringLowHigh     /* String, low then high byte of each word  drvUser=STRING_LOW_HIGH*/
} modbusDataType_t;

#define MAX_MODBUS_DATA_TYPES 15

class epicsShareClass drvModbusAsyn : public asynPortDriver {
public:
    drvModbusAsyn(const char *portName, const char *octetPortName, 
                  int modbusSlave, int modbusFunction, 
                  int modbusStartAddress, int modbusLength,
                  modbusDataType_t dataType,
                  int pollMsec, 
                  const char *plcType);
                 
    /* These are the methods that we override from asynPortDriver */

    /* These functions are in the asynCommon interface */
    virtual void report(FILE *fp, int details);
    virtual asynStatus connect(asynUser *pasynUser);
    virtual asynStatus getAddress(asynUser *pasynUser, int *address);

   /* These functions are in the asynDrvUser interface */
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo, const char **pptypeName, size_t *psize);

    /* These functions are in the asynUInt32Digital interface */
    virtual asynStatus writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask);
    virtual asynStatus readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask);

    /* These functions are in the asynInt32 interface */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);

    /* These functions are in the asynFloat64 interface */
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);

    /* These functions are in the asynInt32Array interface */
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *data, size_t maxChans, size_t *nactual);
    virtual asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *data, size_t maxChans);

    /* These functions are in the asynOctet interface */
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);
    virtual asynStatus readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason);

    /* These are the methods that are new to this class */
    void readPoller();
    modbusDataType_t getDataType(asynUser *pasynUser);
    asynStatus checkOffset(int offset);
    asynStatus checkModbusFunction(int *modbusFunction);
    asynStatus doModbusIO(int slave, int function, int start, epicsUInt16 *data, int len);
    asynStatus readPlcInt(modbusDataType_t dataType, int offset, epicsInt32 *value, int *bufferLen);
    asynStatus writePlcInt(modbusDataType_t dataType, int offset, epicsInt32 value, epicsUInt16 *buffer, int *bufferLen);
    asynStatus readPlcFloat(modbusDataType_t dataType, int offset, epicsFloat64 *value, int *bufferLen);
    asynStatus writePlcFloat(modbusDataType_t dataType, int offset, epicsFloat64  value, epicsUInt16 *buffer, int *bufferLen);
    asynStatus readPlcString (modbusDataType_t dataType, int offset, char *value, size_t maxChars, int *bufferLen);
    asynStatus writePlcString(modbusDataType_t dataType, int offset, const char *value, size_t maxChars, size_t *nActual, int *bufferLen);
    bool modbusExiting_;

protected:
    /** Values used for pasynUser->reason, and indexes into the parameter library. */
    int P_Data;
    int P_Read;
    int P_EnableHistogram;
    int P_ReadHistogram;
    int P_HistogramBinTime;
    int P_HistogramTimeAxis;
    int P_PollDelay;
    int P_ReadOK;
    int P_WriteOK;
    int P_IOErrors;
    int P_LastIOTime;
    int P_MaxIOTime;
 
private:
    /* Our data */
    char *octetPortName_;        /* asyn port name for the asyn octet port */
    char *plcType_;              /* String describing PLC type */
    bool isConnected_;            /* Connection status */
    asynStatus ioStatus_;        /* I/O error status */
    asynUser  *pasynUserOctet_;  /* asynUser for asynOctet interface to asyn octet port */ 
    asynUser  *pasynUserCommon_; /* asynUser for asynCommon interface to asyn octet port */
    asynUser  *pasynUserTrace_;  /* asynUser for asynTrace on this port */
    int modbusSlave_;            /* Modbus slave address */
    int modbusFunction_;         /* Modbus function code */
    int modbusStartAddress_;     /* Modbus starting addess for this port */
    int modbusLength_;           /* Number of words or bits of Modbus data */
    bool absoluteAddressing_;    /* Address from asyn are absolute, rather than relative to modbusStartAddress */
    modbusDataType_t dataType_;  /* Data type */
    epicsUInt16 *data_;          /* Memory buffer */
    char modbusRequest_[MAX_MODBUS_FRAME_SIZE];      /* Modbus request message */
    char modbusReply_[MAX_MODBUS_FRAME_SIZE];        /* Modbus reply message */
    double pollDelay_;           /* Delay for readPoller */
    epicsThreadId readPollerThreadId_;
    epicsEventId readPollerEventId_;
    bool forceCallback_;
    int readOnceFunction_;
    bool readOnceDone_;
    asynStatus prevIOStatus_;
    int readOK_;
    int writeOK_;
    int IOErrors_;
    int currentIOErrors_; /* IO Errors since last successful writeRead cycle */
    int maxIOMsec_;
    int lastIOMsec_;
    epicsInt32 timeHistogram_[HISTOGRAM_LENGTH];     /* Histogram of read-times */
    epicsInt32 histogramTimeAxis_[HISTOGRAM_LENGTH]; /* Time axis of histogram of read-times */
    bool enableHistogram_;
    int histogramMsPerBin_;
    int readbackOffset_;  /* Readback offset for Wago devices */
};

#endif /* drvModbusAsyn_H */
