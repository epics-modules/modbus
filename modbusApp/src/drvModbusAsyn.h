/* drvModbusAsyn.h
 *
 *   Author: Mark Rivers
 *   4-Mar-2007
 *
 *   These are the public definitions for drvModbusAsyn.
 * 
 */

/* These are the strings that device support passes to drivers via 
 * the asynDrvUser interface.
 * Drivers must return a value in pasynUser->reason that is unique 
 * for that command.
 */

#define MODBUS_DATA_STRING             "MODBUS_DATA" 
#define MODBUS_READ_STRING             "MODBUS_READ" 
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
#define MODBUS_ENABLE_HISTOGRAM_STRING "ENABLE_HISTOGRAM"
#define MODBUS_READ_HISTOGRAM_STRING   "READ_HISTOGRAM"
#define MODBUS_HISTOGRAM_BIN_TIME_STRING  "HISTOGRAM_BIN_TIME"
#define MODBUS_HISTOGRAM_TIME_AXIS_STRING "HISTOGRAM_TIME_AXIS"
#define MODBUS_POLL_DELAY_STRING       "POLL_DELAY"
#define MODBUS_READ_OK_STRING          "READ_OK"
#define MODBUS_WRITE_OK_STRING         "WRITE_OK"
#define MODBUS_IO_ERRORS_STRING        "IO_ERRORS"
#define MODBUS_LAST_IO_TIME_STRING     "LAST_IO_TIME"
#define MODBUS_MAX_IO_TIME_STRING      "MAX_IO_TIME"

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


int drvModbusAsynConfigure(char *portName, 
                           char *octetPortName, 
                           int modbusSlave,
                           int modbusFunction, 
                           int modbusStartAddress, 
                           int modbusLength,
                           modbusDataType_t dataType,
                           int pollMsec, 
                           char *plcType);
