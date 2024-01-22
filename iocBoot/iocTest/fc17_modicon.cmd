#!../../bin/linux-x86_64/modbusApp

# fc17_modicon.cmd


< envPaths

dbLoadDatabase("../../dbd/modbusApp.dbd")
modbusApp_registerRecordDeviceDriver(pdbbase)

# Use the following commands for TCP/IP
#drvAsynIPPortConfigure(const char *portName,
#                       const char *hostInfo,
#                       unsigned int priority,
#                       int noAutoConnect,
#                       int noProcessEos);
drvAsynIPPortConfigure("Modicon984","192.168.1.52:4001",0,0,0)
#modbusInterposeConfig(const char *portName,
#                      modbusLinkType linkType,
#                      int timeoutMsec, 
#                      int writeDelayMsec)
modbusInterposeConfig("Modicon984",1,500,0)

#drvModbusAsynConfigure(portName,
#                      tcpPortName,
#                      slaveAddress,
#                      modbusFunction,
#                      modbusStartAddress,
#                      modbusLength,
#                      dataType,
#                      pollMsec,
#                      plcType);
# modbusStartAddress=0 as it doesn't matter in function 17
# modbusLength is device-specific and equal to number of bytes in response of function 17
# Modicon-984 provides 9 bytes in response
drvModbusAsynConfigure("Mod984_slaveID","Modicon984",32,17,0,9,0,1000,"Modicon")

# Enable ASYN_TRACEIO_HEX on octet server
asynSetTraceIOMask("Modicon984",0,4)
# Enable ASYN_TRACE_ERROR and ASYN_TRACEIO_DRIVER on octet server
#asynSetTraceMask("Modicon984",0,9)

# Enable ASYN_TRACEIO_HEX on modbus server
asynSetTraceIOMask("Mod984_slaveID",0,4)
# Enable ASYN_TRACE_ERROR, ASYN_TRACEIO_DEVICE, and ASYN_TRACEIO_DRIVER on modbus server
#asynSetTraceMask("Mod984_slaveID",0,11)

dbLoadTemplate("fc17_modicon.substitutions")

iocInit

