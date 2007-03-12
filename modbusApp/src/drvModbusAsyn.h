/* drvModbusTCPAsyn.h

    Author: Mark Rivers
    4-Mar-2007

    This is device support for Modbus TCP asyn drivers.
  
*/

/* These are the strings that device support passes to drivers via the asynDrvUser interface.
 * Drivers must return a value in pasynUser->reason that is unique for that command.
 */

#define MODBUS_DATA_COMMAND_STRING            "DATA"            /* int32, write (default) */
#define MODBUS_START_HISTOGRAM_COMMAND_STRING "START_HISTOGRAM" /* int32, write */
#define MODBUS_STOP_HISTOGRAM_COMMAND_STRING  "STOP_HISTOGRAM"  /* int32, write */
#define MODBUS_READ_HISTOGRAM_COMMAND_STRING  "READ_HISTOGRAM"  /* int32Array, read */

