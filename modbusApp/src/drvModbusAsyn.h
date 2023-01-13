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
#define MODBUS_INT16_STRING             "INT16"
#define MODBUS_INT16_SM_STRING          "INT16SM"
#define MODBUS_BCD_UNSIGNED_STRING      "BCD_UNSIGNED"
#define MODBUS_BCD_SIGNED_STRING        "BCD_SIGNED"
#define MODBUS_UINT16_STRING            "UINT16"
#define MODBUS_INT32_LE_STRING          "INT32_LE"
#define MODBUS_INT32_LE_BS_STRING       "INT32_LE_BS"
#define MODBUS_INT32_BE_STRING          "INT32_BE"
#define MODBUS_INT32_BE_BS_STRING       "INT32_BE_BS"
#define MODBUS_UINT32_LE_STRING         "UINT32_LE"
#define MODBUS_UINT32_LE_BS_STRING      "UINT32_LE_BS"
#define MODBUS_UINT32_BE_STRING         "UINT32_BE"
#define MODBUS_UINT32_BE_BS_STRING      "UINT32_BE_BS"
#define MODBUS_INT64_LE_STRING          "INT64_LE"
#define MODBUS_INT64_LE_BS_STRING       "INT64_LE_BS"
#define MODBUS_INT64_BE_STRING          "INT64_BE"
#define MODBUS_INT64_BE_BS_STRING       "INT64_BE_BS"
#define MODBUS_UINT64_LE_STRING         "UINT64_LE"
#define MODBUS_UINT64_LE_BS_STRING      "UINT64_LE_BS"
#define MODBUS_UINT64_BE_STRING         "UINT64_BE"
#define MODBUS_UINT64_BE_BS_STRING      "UINT64_BE_BS"
#define MODBUS_FLOAT32_LE_STRING        "FLOAT32_LE"
#define MODBUS_FLOAT32_LE_BS_STRING     "FLOAT32_LE_BS"
#define MODBUS_FLOAT32_BE_STRING        "FLOAT32_BE"
#define MODBUS_FLOAT32_BE_BS_STRING     "FLOAT32_BE_BS"
#define MODBUS_FLOAT64_LE_STRING        "FLOAT64_LE"
#define MODBUS_FLOAT64_LE_BS_STRING     "FLOAT64_LE_BS"
#define MODBUS_FLOAT64_BE_STRING        "FLOAT64_BE"
#define MODBUS_FLOAT64_BE_BS_STRING     "FLOAT64_BE_BS"
#define MODBUS_STRING_HIGH_STRING       "STRING_HIGH"
#define MODBUS_STRING_LOW_STRING        "STRING_LOW"
#define MODBUS_STRING_HIGH_LOW_STRING   "STRING_HIGH_LOW"
#define MODBUS_STRING_LOW_HIGH_STRING   "STRING_LOW_HIGH"
#define MODBUS_ZSTRING_HIGH_STRING      "ZSTRING_HIGH"
#define MODBUS_ZSTRING_LOW_STRING       "ZSTRING_LOW"
#define MODBUS_ZSTRING_HIGH_LOW_STRING  "ZSTRING_HIGH_LOW"
#define MODBUS_ZSTRING_LOW_HIGH_STRING  "ZSTRING_LOW_HIGH"

#define HISTOGRAM_LENGTH 200  /* Length of time histogram */

typedef enum {
    dataTypeInt16,
    dataTypeInt16SM,
    dataTypeBCDUnsigned,
    dataTypeBCDSigned,
    dataTypeUInt16,
    dataTypeInt32LE,
    dataTypeInt32LEBS,
    dataTypeInt32BE,
    dataTypeInt32BEBS,
    dataTypeUInt32LE,
    dataTypeUInt32LEBS,
    dataTypeUInt32BE,
    dataTypeUInt32BEBS,
    dataTypeInt64LE,
    dataTypeInt64LEBS,
    dataTypeInt64BE,
    dataTypeInt64BEBS,
    dataTypeUInt64LE,
    dataTypeUInt64LEBS,
    dataTypeUInt64BE,
    dataTypeUInt64BEBS,
    dataTypeFloat32LE,
    dataTypeFloat32LEBS,
    dataTypeFloat32BE,
    dataTypeFloat32BEBS,
    dataTypeFloat64LE,
    dataTypeFloat64LEBS,
    dataTypeFloat64BE,
    dataTypeFloat64BEBS,
    dataTypeStringHigh,
    dataTypeStringLow,
    dataTypeStringHighLow,
    dataTypeStringLowHigh,
    dataTypeZStringHigh,
    dataTypeZStringLow,
    dataTypeZStringHighLow,
    dataTypeZStringLowHigh,
    MAX_MODBUS_DATA_TYPES
} modbusDataType_t;

struct modbusDrvUser_t;

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
    virtual asynStatus drvUserDestroy(asynUser *pasynUser);

    /* These functions are in the asynUInt32Digital interface */
    virtual asynStatus writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask);
    virtual asynStatus readUInt32Digital(asynUser *pasynUser, epicsUInt32 *value, epicsUInt32 mask);

    /* These functions are in the asynInt32 interface */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);

    /* These functions are in the asynInt64 interface */
    virtual asynStatus writeInt64(asynUser *pasynUser, epicsInt64 value);
    virtual asynStatus readInt64(asynUser *pasynUser, epicsInt64 *value);

    /* These functions are in the asynFloat64 interface */
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);

    /* These functions are in the asynFloat64Array interface */
    virtual asynStatus readFloat64Array(asynUser *pasynUser, epicsFloat64 *data, size_t maxChans, size_t *nactual);
    virtual asynStatus writeFloat64Array(asynUser *pasynUser, epicsFloat64 *data, size_t maxChans);

    /* These functions are in the asynInt32Array interface */
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *data, size_t maxChans, size_t *nactual);
    virtual asynStatus writeInt32Array(asynUser *pasynUser, epicsInt32 *data, size_t maxChans);

    /* These functions are in the asynOctet interface */
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);
    virtual asynStatus readOctet(asynUser *pasynUser, char *value, size_t maxChars, size_t *nActual, int *eomReason);

    /* These are the methods that are new to this class */
    void readPoller();
    modbusDataType_t getDataType(asynUser *pasynUser);
    int getStringLen(asynUser *pasynUser, size_t maxChars);
    bool isZeroTerminatedString(modbusDataType_t dataType);
    asynStatus checkOffset(int offset);
    asynStatus checkModbusFunction(int *modbusFunction);
    asynStatus doModbusIO(int slave, int function, int start, epicsUInt16 *data, int len);
    asynStatus readPlcInt32(modbusDataType_t dataType, int offset, epicsInt32 *value, int *bufferLen);
    asynStatus writePlcInt32(modbusDataType_t dataType, int offset, epicsInt32 value, epicsUInt16 *buffer, int *bufferLen);
    asynStatus readPlcInt64(modbusDataType_t dataType, int offset, epicsInt64 *value, int *bufferLen);
    asynStatus writePlcInt64(modbusDataType_t dataType, int offset, epicsInt64 value, epicsUInt16 *buffer, int *bufferLen);
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
    bool initialized_;           /* If initialized successfully */
    char *octetPortName_;        /* asyn port name for the asyn octet port */
    char *plcType_;              /* String describing PLC type */
    bool isConnected_;            /* Connection status */
    asynStatus ioStatus_;        /* I/O error status */
    asynStatus prevIOStatus_;    /* Previous I/O error status */
    asynUser  *pasynUserOctet_;  /* asynUser for asynOctet interface to asyn octet port */
    asynUser  *pasynUserCommon_; /* asynUser for asynCommon interface to asyn octet port */
    asynUser  *pasynUserTrace_;  /* asynUser for asynTrace on this port */
    int modbusSlave_;            /* Modbus slave address */
    int modbusFunction_;         /* Modbus function code */
    int modbusStartAddress_;     /* Modbus starting addess for this port */
    int modbusLength_;           /* Number of words or bits of Modbus data */
    bool absoluteAddressing_;    /* Address from asyn are absolute, rather than relative to modbusStartAddress */
    modbusDataType_t dataType_;  /* Data type */
    modbusDrvUser_t *drvUser_;   /* Drv user structure */
    epicsUInt16 *data_;          /* Memory buffer */
    char modbusRequest_[MAX_MODBUS_FRAME_SIZE];      /* Modbus request message */
    char modbusReply_[MAX_MODBUS_FRAME_SIZE];        /* Modbus reply message */
    double pollDelay_;           /* Delay for readPoller */
    epicsThreadId readPollerThreadId_;
    epicsEventId readPollerEventId_;
    bool forceCallback_;
    int readOnceFunction_;
    bool readOnceDone_;
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
