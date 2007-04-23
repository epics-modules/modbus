# st.cmd for modbus
< envPaths

dbLoadDatabase("../../dbd/modbus.dbd")
modbus_registerRecordDeviceDriver(pdbbase)

# Use the following commands for TCP/IP
#drvAsynIPPortConfigure(const char *portName,
#                       const char *hostInfo,
#                       unsigned int priority,
#                       int noAutoConnect,
#                       int noProcessEos);
#drvAsynIPPortConfigure("Koyo2","164.54.160.38:502",0,1,1)
#modbusInterposeConfig(const char *portName,
#                      int slaveAddress,
#                      modbusLinkType linkType)
modbusInterposeConfig("Koyo2",0,0)

# Use the following commands for serial RTU or ASCII
#drvAsynSerialPortConfigure(const char *portName,
#                           const char *ttyName,
#                           unsigned int priority,
#                           int noAutoConnect,
#                           int noProcessEos);
drvAsynSerialPortConfigure("Koyo2", "/dev/ttyS1", 0, 0, 0)
asynSetOption("Koyo2",0,"baud","38400")
asynSetOption("Koyo2",0,"parity","none")
asynSetOption("Koyo2",0,"bits","8")
asynSetOption("Koyo2",0,"stop","1")

# Use the following command for serial RTU
modbusInterposeConfig("Koyo2",1,1)

# Use the following commands for serial ASCII
#asynOctetSetOutputEos("Koyo2",0,"\r\n")
#asynOctetSetInputEos("Koyo2",0,"\r\n")
#modbusInterposeConfig("Koyo2",1,2)

# Trace flags for asynOctet driver
asynSetTraceIOMask("Koyo2",0,4)
#asynSetTraceMask("Koyo2",0,9)

# NOTE: We use octal numbers for the start address and length (leading zeros)
#       to be consistent with the PLC nomenclature.  This is optional, decimal
#       numbers (no leading zero) or hex numbers can also be used.

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Read 32 bits (Y0-Y37).  Function code=1.
drvModbusAsynConfigure("K2_Yn_In_Bit",       "Koyo2",    1,  04000,  040,    0,  100,    "Koyo")

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Write 32 bits (Y0-Y37).  Function code=5.
drvModbusAsynConfigure("K2_Yn_Out_Bit",      "Koyo2",    5,  04000,  040,    0,  1,      "Koyo")

# The DL205 has bit access to the Cn bits at Modbus offset 6000 (octal)
# Access 256 bits (C0-C377) as inputs.  Function code=1.
drvModbusAsynConfigure("K2_Cn_In_Bit",       "Koyo2",    1,  06000,  0400,   0,  100,    "Koyo")

# Access the same 256 bits (C0-C377) as outputs.  Function code=5.
drvModbusAsynConfigure("K2_Cn_Out_Bit",      "Koyo2",    5,  06000,  0400,   0,  1,      "Koyo")

# The DL205 has word access to the V3000 memory at Modbus offset 3000 (octal)
# Access 32 words (V3000-V3040) as inputs.  Function code=3, data type signed BCD.
drvModbusAsynConfigure("K2_V3000_In_Word",   "Koyo2",    3,  03000,  040,    3,  100,    "Koyo")

# Hex trace format on octet server
asynSetTraceIOMask("Koyo2",0,4)
# Turn asynTraceIODriver on octet server
#asynSetTraceMask("Koyo2",0,9)

# Hex trace format on modbus server
asynSetTraceIOMask("K2_V3000_In_Word",0,4)
# Turn on all debugging on modbus server
#asynSetTraceMask("K2_V3000_In_Word",0,255)

dbLoadTemplate("Koyo2.substitutions")

iocInit

