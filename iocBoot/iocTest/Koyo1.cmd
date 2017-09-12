# Koyo1.cmd

dbLoadDatabase("../../dbd/modbus.dbd")
modbus_registerRecordDeviceDriver(pdbbase)

# Use the following commands for TCP/IP
#drvAsynIPPortConfigure(const char *portName, 
#                       const char *hostInfo,
#                       unsigned int priority, 
#                       int noAutoConnect,
#                       int noProcessEos);
drvAsynIPPortConfigure("Koyo1","164.54.160.158:502",0,0,1)
asynSetOption("Koyo1",0, "disconnectOnReadTimeout", "Y")
m#modbusInterposeConfig(const char *portName, 
#                      modbusLinkType linkType,
#                      int timeoutMsec, 
#                      int writeDelayMsec)
modbusInterposeConfig("Koyo1",0,5000,0)

# Use the following commands for serial RTU or ASCII
#drvAsynSerialPortConfigure(const char *portName, 
#                           const char *ttyName,
#                           unsigned int priority, 
#                           int noAutoConnect,
#                           int noProcessEos);
#drvAsynSerialPortConfigure("Koyo1", "/dev/ttyS1", 0, 0, 0)
#asynSetOption("Koyo1",0,"baud","38400")
#asynSetOption("Koyo1",0,"parity","none")
#asynSetOption("Koyo1",0,"bits","8")
#asynSetOption("Koyo1",0,"stop","1")

# Use the following command for serial RTU
# Note: non-zero write delay (last parameter) may be needed.
#modbusInterposeConfig("Koyo1",1,1000,0)

# Use the following commands for serial ASCII
#asynOctetSetOutputEos("Koyo1",0,"\r\n")
#asynOctetSetInputEos("Koyo1",0,"\r\n")
# Note: non-zero write delay (last parameter) may be needed.
#modbusInterposeConfig("Koyo1",2,1000,0)

# NOTE: We use octal numbers for the start address and length (leading zeros)
#       to be consistent with the PLC nomenclature.  This is optional, decimal
#       numbers (no leading zero) or hex numbers can also be used.
#       In these examples we are using slave address 0 (number after "Koyo1").

# The DL205 has bit access to the Xn inputs at Modbus offset 4000 (octal)
# Read 32 bits (X0-X37).  Function code=2.
drvModbusAsynConfigure("K1_Xn_Bit",      "Koyo1", 0, 2,  04000, 040,    0,  100, "Koyo")

# The DL205 has word access to the Xn inputs at Modbus offset 40400 (octal)
# Read 8 words (128 bits).  Function code=3.
drvModbusAsynConfigure("K1_Xn_Word",     "Koyo1", 0, 3, 040400, 010,    0,  100, "Koyo")

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Read 32 bits (Y0-Y37).  Function code=1.
drvModbusAsynConfigure("K1_Yn_In_Bit",   "Koyo1", 0, 1,  04000, 040,    0,  100, "Koyo")

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Write 32 bits (Y0-Y37).  Function code=5.
drvModbusAsynConfigure("K1_Yn_Out_Bit",  "Koyo1", 0, 5,  04000, 040,    0,  1, "Koyo")

# The DL205 has word access to the Yn outputs at Modbus offset 40500 (octal)
# Read 8 words (128 bits).  Function code=3.
drvModbusAsynConfigure("K1_Yn_In_Word",  "Koyo1", 0, 3, 040500, 010,    0,  100, "Koyo")

# Write 8 words (128 bits).  Function code=6.
drvModbusAsynConfigure("K1_Yn_Out_Word", "Koyo1", 0, 6, 040500, 010,    0,  100, "Koyo")

# The DL205 has bit access to the Cn bits at Modbus offset 6000 (octal)
# Access 256 bits (C0-C377) as inputs.  Function code=1.
drvModbusAsynConfigure("K1_Cn_In_Bit",   "Koyo1", 0, 1,  06000, 0400,   0,  100, "Koyo")

# Access the same 256 bits (C0-C377) as outputs.  Function code=5.
drvModbusAsynConfigure("K1_Cn_Out_Bit",  "Koyo1", 0, 5,  06000, 0400,   0,  1,  "Koyo")

# Access the same 256 bits (C0-C377) as array outputs.  Function code=15.
drvModbusAsynConfigure("K1_Cn_Out_Bit_Array",  "Koyo1", 0, 15,  06000, 0400,   0,   1, "Koyo")

# The DL205 has word access to the Cn bits at Modbus offset 40600 (octal)
# We use the first 16 words (C0-C377) as inputs (256 bits).  Function code=3.
drvModbusAsynConfigure("K1_Cn_In_Word",  "Koyo1", 0, 3, 040600, 020,    0,  100, "Koyo")

# We access the same 16 words (C0-C377) as outputs (256 bits). Function code=6.
drvModbusAsynConfigure("K1_Cn_Out_Word", "Koyo1", 0, 6, 040600, 020,    0,  1,  "Koyo")

# We access the same 16 words (C0-C377) as array outputs (256 bits). Function code=16.
drvModbusAsynConfigure("K1_Cn_Out_Word_Array", "Koyo1", 0, 16, 040600, 020,    0,   1, "Koyo")

# Enable ASYN_TRACEIO_HEX on octet server
asynSetTraceIOMask("Koyo1",0,4)
# Enable ASYN_TRACE_ERROR and ASYN_TRACEIO_DRIVER on octet server
#asynSetTraceMask("Koyo1",0,9)

# Enable ASYN_TRACEIO_HEX on modbus server
asynSetTraceIOMask("K1_Yn_In_Bit",0,4)
# Enable all debugging on modbus server
#asynSetTraceMask("K1_Yn_In_Bit",0,255)
# Dump up to 512 bytes in asynTrace
asynSetTraceIOTruncateSize("K1_Yn_In_Bit",0,512)

dbLoadTemplate("Koyo1.substitutions")

iocInit

