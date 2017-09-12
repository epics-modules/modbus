# simulator.cmd

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

# Use the following commands for serial RTU or ASCII
#drvAsynSerialPortConfigure(const char *portName,
#                           const char *ttyName,
#                           unsigned int priority,
#                           int noAutoConnect,
#                           int noProcessEos);
#drvAsynSerialPortConfigure("sim1", "/dev/ttyS1", 0, 0, 0)
#asynSetOption("sim1",0,"baud","38400")
#asynSetOption("sim1",0,"parity","none")
#asynSetOption("sim1",0,"bits","8")
#asynSetOption("sim1",0,"stop","1")

# Use the following command for serial RTU. 
# Note: non-zero write delay (last parameter) may be needed.
#modbusInterposeConfig("sim1",1,2000,0)

# Use the following commands for serial ASCII
#asynOctetSetOutputEos("sim1",0,"\r\n")
#asynOctetSetInputEos("sim1",0,"\r\n")
# Note: non-zero write delay (last parameter) may be needed.
#modbusInterposeConfig("sim1",2,2000,0)

# Bit access at Modbus address 0
# Access 128 bits as inputs.  
# Function code=1
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("A0_In_Bits", "sim1", 0, 1,  0, 128, 0, 100, "Simulator")

# Bit access at Modbus address 0
# Access 128 bits as outputs.  
# Function code=5
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("A0_Out_Bits", "sim1", 0, 5,  0, 128, 0, 100, "Simulator")

# Access 60 words as outputs.  
# Either function code=6 (single register) or 16 (multiple registers) can be used, but 16
# is better because it is "atomic" when writing values longer than 16-bits.
# Default data type unsigned integer.
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("A0_Out_Word", "sim1", 0, 16, 100, 60, 0, 1, "Simulator")

# Word access at Modbus address 100
# Access 60 words as inputs.  
# Function code=3
# default data type unsigned integer.
# drvModbusAsynConfigure("portName", "tcpPortName", slaveAddress, modbusFunction, modbusStartAddress, modbusLength, dataType, pollMsec, "plcType")
drvModbusAsynConfigure("A0_In_Word", "sim1", 0, 3, 100, 60, 0, 100, "Simulator")

# Enable ASYN_TRACEIO_HEX on octet server
asynSetTraceIOMask("sim1",0,4)
# Enable ASYN_TRACE_ERROR and ASYN_TRACEIO_DRIVER on octet server
#asynSetTraceMask("sim1",0,9)

dbLoadTemplate("sim1.substitutions")

iocInit

