// This program tests creating a new drvModbusAsyn object and using it to do I/O using absolute addressing from
// a pure C++ program without an IOC.

#include <stdio.h>
#include <string.h>

#include <shareLib.h>

#include <drvAsynIPPort.h>
#include <asynShellCommands.h>

#include <drvModbusAsyn.h>
#include <modbusInterpose.h>


int main(int argc, char *argv[])
{
    epicsUInt16 data[256];
    int i;

    /* Use the following commands for TCP/IP
     * drvAsynIPPortConfigure(const char *portName, 
     *                        const char *hostInfo,
     *                        unsigned int priority, 
     *                        int noAutoConnect,
     *                        int noProcessEos); */
    drvAsynIPPortConfigure("Koyo1","camaro:502",0,0,0);
    asynSetOption("Koyo1", 0, "disconnectOnReadTimeout", "Y");

    /* modbusInterposeConfig(const char *portName, 
     *                       modbusLinkType linkType,
     *                       int timeoutMsec, 
     *                       int writeDelayMsec) */
    modbusInterposeConfig("Koyo1", modbusLinkTCP, 5000, 0);

    /* drvModbusAsyn(const char *portName, const char *octetPortName, 
     *               int modbusSlave, int modbusFunction, 
     *               int modbusStartAddress, int modbusLength,
     *               modbusDataType_t dataType,
     *               int pollMsec, 
     *               const char *plcType); */
    // Use absolute addressing, modbusStartAddress=-1.
    drvModbusAsyn *pModbus = new drvModbusAsyn("K1", "Koyo1", 0, 2, -1, 256, dataTypeUInt16, 0, "Koyo");
    
    // Write 10 bits at address 2048
    memset(data, 0, sizeof(data));
    data[0] = 1;
    data[2] = 1;
    data[4] = 1;
    data[6] = 1;
    data[8] = 1;
    printf("  Writing [1 0 1 0 1 0 1 0 1 0] to adddress 2048\n");
    /* asynStatus doModbusIO(int slave, int function, int start, epicsUInt16 *data, int len); */
    pModbus->doModbusIO(0, MODBUS_WRITE_MULTIPLE_COILS, 2048, data, 10);
    
    // Read them back
    memset(data, 0, sizeof(data));
    pModbus->doModbusIO(0, MODBUS_READ_COILS, 2048, data, 10);
    printf("Read back [");
    for (i=0; i<10; i++) printf("%d ", data[i]);
    printf("] from address 2048\n");
    
    // Write 10 words at address 3072
    for (i=0; i<10; i++) data[i] = i;
    printf("  Writing [0 1 2 3 4 5 6 7 8 9] to adddress 3072\n");
    pModbus->doModbusIO(0, MODBUS_WRITE_MULTIPLE_REGISTERS, 3072, data, 10);
    
    // Read them back
    memset(data, 0, sizeof(data));
    pModbus->doModbusIO(0, MODBUS_READ_HOLDING_REGISTERS, 3072, data, 10);
    printf("Read back [");
    for (i=0; i<10; i++) printf("%d ", data[i]);
    printf("] from address 3072\n");
    
}