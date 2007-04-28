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

#define MODBUS_DATA_COMMAND_STRING             "MODBUS_DATA" 
#define MODBUS_ENABLE_HISTOGRAM_COMMAND_STRING "ENABLE_HISTOGRAM"
#define MODBUS_READ_HISTOGRAM_COMMAND_STRING   "READ_HISTOGRAM"
#define MODBUS_POLL_DELAY_COMMAND_STRING       "POLL_DELAY"
#define MODBUS_READ_OK_COMMAND_STRING          "READ_OK"
#define MODBUS_WRITE_OK_COMMAND_STRING         "WRITE_OK"
#define MODBUS_IO_ERRORS_COMMAND_STRING        "IO_ERRORS"
#define MODBUS_LAST_IO_TIME_COMMAND_STRING     "LAST_IO_TIME"
#define MODBUS_MAX_IO_TIME_COMMAND_STRING      "MAX_IO_TIME"

typedef enum {
    dataTypeBinary,
    dataTypeSignedBinary,
    dataTypeBCD,
    dataTypeSignedBCD
} modbusDataType;

int drvModbusAsynConfigure(char *portName, 
                           char *octetPortName, 
                           int modbusFunction, 
                           int modbusStartAddress, 
                           int modbusLength,
                           modbusDataType dataType,
                           int pollMsec, 
                           char *plcType);
