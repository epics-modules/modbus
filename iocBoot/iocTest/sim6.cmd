# sim5.cmd
# This tests using absolute Modbus addresses.

< envPaths

dbLoadDatabase("../../dbd/modbus.dbd")
modbus_registerRecordDeviceDriver(pdbbase)

# Use the following commands for TCP/IP
#drvAsynIPPortConfigure(const char *portName,
#                       const char *hostInfo,
#                       unsigned int priority,
#                       int noAutoConnect,
#                       int noProcessEos);
drvAsynIPPortConfigure("sim1","164.54.160.31:502",0,0,1)
asynSetOption("sim1",0, "disconnectOnReadTimeout", "Y")

#modbusInterposeConfig(const char *portName,
#                      modbusLinkType linkType,
#                      int timeoutMsec, 
#                      int writeDelayMsec)
modbusInterposeConfig("sim1",0,2000,0)

# Word write access, Function code 16 (multiple registers)
# Default data type unsigned integer.
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("OUT1", "sim1", 0, 16, 2000, 123, 0, 100, "Simulator")
drvModbusAsynConfigure("OUT2", "sim1", 0, 16, 3000, 123, 0, 100, "Simulator")
drvModbusAsynConfigure("OUT3", "sim1", 0, 16, 4000, 123, 0, 100, "Simulator")

# Word read access, Function code 3.  
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("IN1", "sim1", 0, 3,  2000, 123, 0, 100, "Simulator")
drvModbusAsynConfigure("IN2", "sim1", 0, 3,  3000, 123, 0, 100, "Simulator")
drvModbusAsynConfigure("IN3", "sim1", 0, 3,  4000, 123, 0, 100, "Simulator")

# Enable ASYN_TRACEIO_HEX on octet server
asynSetTraceIOMask("sim1",0,4)
# Enable ASYN_TRACE_ERROR and ASYN_TRACEIO_DRIVER on octet server
#asynSetTraceMask("sim1",0,9)

dbLoadTemplate("sim6.substitutions")

iocInit

