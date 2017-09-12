# sim4.cmd

<envPaths

dbLoadDatabase("../../dbd/modbus.dbd")
modbus_registerRecordDeviceDriver(pdbbase)

# Use the following commands for TCP/IP
#drvAsynIPPortConfigure(const char *portName,
#                       const char *hostInfo,
#                       unsigned int priority,
#                       int noAutoConnect,
#                       int noProcessEos);
# This line is for Modbus TCP
#drvAsynIPPortConfigure("sim1","164.54.160.31:502",0,0,1)
# These 3 lines are for Modbus ASCII using Moxa terminal server connection
drvAsynIPPortConfigure("sim1","164.54.160.187:2101",0,0,)
asynSetOption("sim1",0, "disconnectOnReadTimeout", "Y")
asynOctetSetOutputEos("sim1",0,"\r\n")
asynOctetSetInputEos("sim1",0,"\r\n")
#modbusInterposeConfig(const char *portName,
#                      modbusLinkType linkType,
#                      int timeoutMsec, 
#                      int writeDelayMsec)
# This line is for Modbus TCP
#modbusInterposeConfig("sim1",0,2000,0)
# This line is for Modbus ASCII
modbusInterposeConfig("sim1",2,2000,0)

# Word access at Modbus address 0
# Access 1 words as inputs.  
# Function code=23, which is 123 for EPICS modbus driver
# default data type unsigned integer.
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("A0_In_Word", "sim1", 0, 123, 0, 4, 0, 1000, "Simulator")

# Access 1 words as outputs.  
# Function code=23, which is 223 for EPICS modbus driver
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("A0_Out_Word", "sim1", 0,223, 0, 4, 0, 1, "Simulator")

# Enable ASYN_TRACEIO_HEX on octet server
#asynSetTraceIOMask("sim1",0,4)
# Enable ASYN_TRACEIO_ESCAPE on octet server
asynSetTraceIOMask("sim1",0,2)
# Enable ASYN_TRACE_ERROR and ASYN_TRACEIO_DRIVER on octet server
#asynSetTraceMask("sim1",0,9)

dbLoadTemplate("sim4.substitutions")

iocInit

