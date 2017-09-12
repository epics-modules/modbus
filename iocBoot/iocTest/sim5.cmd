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

# Bit read access, Function Code 1
# Function code=1
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("FC1", "sim1", 0, 1,  -1, 8, 0, 100, "Simulator")

# Bit write access, Function Code 5
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("FC5", "sim1", 0, 5,  -1, 1, 0, 100, "Simulator")

# Word write access, Function code 6 (single register)
# Default data type unsigned integer.
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("FC6", "sim1", 0, 6, -1, 4, 0, 100, "Simulator")

# Word read access, Function code 3.  
# Set length to 4 words so 64-bit floats can be read
# Default data type unsigned integer.
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("FC3", "sim1", 0, 3,  -1, 4, 0, 100, "Simulator")

# Enable ASYN_TRACEIO_HEX on octet server
asynSetTraceIOMask("sim1",0,4)
# Enable ASYN_TRACE_ERROR and ASYN_TRACEIO_DRIVER on octet server
#asynSetTraceMask("sim1",0,9)

dbLoadTemplate("sim5.substitutions")

iocInit

