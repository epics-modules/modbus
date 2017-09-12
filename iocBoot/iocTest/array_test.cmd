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

# Word access at Modbus address 0
# Access 20 words as UINT16 data type.  
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("UINT16_F3",   "sim1", 0, 3,  0, 20, 0, 100, "Simulator")
drvModbusAsynConfigure("UINT16_F16",  "sim1", 0,16,  0, 20, 0, 100, "Simulator")

# Word access at Modbus address 20
# Access 20 words as INT16 data type.  
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("INT16_F3",    "sim1", 0, 3, 20, 20, 4, 100, "Simulator")
drvModbusAsynConfigure("INT16_F16",   "sim1", 0,16, 20, 20, 4, 100, "Simulator")

# Word access at Modbus address 40
# Access 20 words as INT32_LE data type.  
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("INT32_LE_F3", "sim1", 0, 3, 40, 20, 5, 100, "Simulator")
drvModbusAsynConfigure("INT32_LE_F16","sim1", 0,16, 40, 20, 5, 100, "Simulator")

# Word access at Modbus address 60
# Access 20 words as INT32_BE data type.  
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("INT32_BE_F3", "sim1", 0, 3, 60, 20, 6, 100, "Simulator")
drvModbusAsynConfigure("INT32_BE_F16","sim1", 0,16, 60, 20, 6, 100, "Simulator")


# Enable ASYN_TRACEIO_HEX on octet server
asynSetTraceIOMask("sim1",0,4)
# Enable ASYN_TRACE_ERROR and ASYN_TRACEIO_DRIVER on octet server
#asynSetTraceMask("sim1",0,9)

dbLoadTemplate("array_test.substitutions")

iocInit

