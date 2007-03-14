# st.cmd for modbusTCP

dbLoadDatabase("../../dbd/modbusTCP.dbd")
modbusTCP_registerRecordDeviceDriver(pdbbase)

drvAsynIPPortConfigure("Koyu1","164.54.160.158:502",0,0,1)

# drvModbusTCPAsynConfigure(
# "Port name"
# "TCP/IP port name"
# " PLC type"iocshArgString};
# "Modbus function code" (1, 2, 3 ...)
# "Modbus start address"
# "Modbus length"
# "Poll time (msec)"

# NOTE: We use octal numbers for the start address and length (leading zeros)
#       to be consistent with the PLC nomenclature.  This is optional, decimal
#       numbers (no leading zero) can also be used.

# The DL205 has bit access to the Xn inputs at Modbus offset 4000 (octal)
# Read 32 bits (X0-X37).  Function code=2.
drvModbusTCPAsynConfigure("K1_Xn_Bit", "Koyu1", "Koyu", 2, 04000, 040, 100)
epicsThreadSleep(0.2)

# The DL205 has word access to the Xn inputs at Modbus offset 40400 (octal)
# Read 8 words (128 bits).  Function code=3.
drvModbusTCPAsynConfigure("K1_Xn_Word", "Koyu1", "Koyu", 3, 040400, 010, 100)
epicsThreadSleep(0.2)

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Read 32 bits (Y0-Y37).  Function code=1.
drvModbusTCPAsynConfigure("K1_Yn_In_Bit", "Koyu1", "Koyu", 1, 04000, 040, 100)
epicsThreadSleep(0.2)

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Write 32 bits (Y0-Y37).  Function code=5.
drvModbusTCPAsynConfigure("K1_Yn_Out_Bit", "Koyu1", "Koyu", 5, 04000, 040, 1)
epicsThreadSleep(0.2)

 The DL205 has word access to the Yn outputs at Modbus offset 40500 (octal)
# Read 8 words (128 bits).  Function code=3.
drvModbusTCPAsynConfigure("K1_Yn_In_Word", "Koyu1", "Koyu", 3, 040500, 010, 100)
epicsThreadSleep(0.2)

# Write 8 words (128 bits).  Function code=6.
drvModbusTCPAsynConfigure("K1_Yn_Out_Word", "Koyu1", "Koyu", 6, 040500, 010, 100)
epicsThreadSleep(0.2)

# The DL205 has bit access to the Cn bits at Modbus offset 6000 (octal)
# Access 256 bits (C0-C377) as inputs.  Function code 1.
drvModbusTCPAsynConfigure("K1_Cn_In_Bit", "Koyu1", "Koyu", 1, 06000, 0400, 100)
epicsThreadSleep(0.2)

# Acces the same 256 bits (C0-C377) as outputs.  Function code 5.
drvModbusTCPAsynConfigure("K1_Cn_Out_Bit", "Koyu1", "Koyu", 5, 06000, 0400, 1)
epicsThreadSleep(0.2)

# The DL205 has word access to the Cn bits at Modbus offset 40600 (octal)
# We use the first 16 words (C0-C377) as inputs (256 bits)
drvModbusTCPAsynConfigure("K1_Cn_In_Word", "Koyu1", "Koyu", 3, 040600, 020, 100)
epicsThreadSleep(0.2)

# We access the same 16 words (C0-C377) as outputs (256 bits)
drvModbusTCPAsynConfigure("K1_Cn_Out_Word", "Koyu1", "Koyu", 6, 040600, 020, 1)
epicsThreadSleep(0.2)

# Hex trace format on TCP server
asynSetTraceIOMask("Koyu1",0,4)
# Turn on all debugging on TCP server
#asynSetTraceMask("Koyu1",0,255)

# Hex trace format on modbusTCP server
asynSetTraceIOMask("K1_Xn_Word",0,4)
# Turn on all debugging on TCP server
#asynSetTraceMask("K1_Xn_Word",0,255)

dbLoadTemplate("K1_Test.substitutions")

iocInit

