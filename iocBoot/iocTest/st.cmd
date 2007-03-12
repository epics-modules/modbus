# st.cmd for modbusTCP

dbLoadDatabase("../../dbd/modbusTCP.dbd")
modbusTCP_registerRecordDeviceDriver(pdbbase)

drvAsynIPPortConfigure("GAS","164.54.160.158:502",0,0,1)

# drvModbusTCPAsynConfigure(
# "Port name"
# "TCP/IP port name"
# " PLC type"iocshArgString};
# "Modbus function code" (1, 2, 3 ...)
# "Modbus start address"
# "Modbus length"
# "Poll time (msec)"

# The DL205 has the X0 inputs at Modbus offset 40400 (octal)
drvModbusTCPAsynConfigure("GAS_Xn", "GAS", "Koyu", 3, 040400, 16, 100)

# Hex trace format on TCP server
asynSetTraceIOMask("GAS",0,4)
# Turn on all debugging on TCP server
#asynSetTraceMask("GAS",0,255)

# Hex trace format on modbusTCP server
asynSetTraceIOMask("GAS_Xn",0,4)
# Turn on all debugging on TCP server
#asynSetTraceMask("GAS_Xn",0,255)

dbLoadTemplate("PLC_GAS.substitutions")

iocInit

