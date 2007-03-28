# st.cmd for modbusTCP

dbLoadDatabase("../../dbd/modbusTCP.dbd")
modbusTCP_registerRecordDeviceDriver(pdbbase)

drvAsynIPPortConfigure("Koyo1","164.54.160.158:502",0,0,1)
#drvAsynIPPortConfigure("Koyo1","164.54.160.201:502",0,0,1)


# NOTE: We use octal numbers for the start address and length (leading zeros)
#       to be consistent with the PLC nomenclature.  This is optional, decimal
#       numbers (no leading zero) or hex numbers can also be used.

# The DL205 has bit access to the Xn inputs at Modbus offset 4000 (octal)
# Read 32 bits (X0-X37).  Function code=2.
drvModbusTCPAsynConfigure("K1_Xn_Bit",      "Koyo1", 2,  04000, 040,    0,  100, "Koyo")

# The DL205 has word access to the Xn inputs at Modbus offset 40400 (octal)
# Read 8 words (128 bits).  Function code=3.
drvModbusTCPAsynConfigure("K1_Xn_Word",     "Koyo1", 3, 040400, 010,    0,  100, "Koyo")

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Read 32 bits (Y0-Y37).  Function code=1.
drvModbusTCPAsynConfigure("K1_Yn_In_Bit",   "Koyo1", 1,  04000, 040,    0,  100, "Koyo")

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Write 32 bits (Y0-Y37).  Function code=5.
drvModbusTCPAsynConfigure("K1_Yn_Out_Bit",  "Koyo1", 5,  04000, 040,    0,  1, "Koyo")

# The DL205 has word access to the Yn outputs at Modbus offset 40500 (octal)
# Read 8 words (128 bits).  Function code=3.
drvModbusTCPAsynConfigure("K1_Yn_In_Word",  "Koyo1", 3, 040500, 010,    0,  100, "Koyo")

# Write 8 words (128 bits).  Function code=6.
drvModbusTCPAsynConfigure("K1_Yn_Out_Word", "Koyo1", 6, 040500, 010,    0,  100, "Koyo")

# The DL205 has bit access to the Cn bits at Modbus offset 6000 (octal)
# Access 256 bits (C0-C377) as inputs.  Function code=1.
drvModbusTCPAsynConfigure("K1_Cn_In_Bit",   "Koyo1", 1,  06000, 0400,   0,  100, "Koyo")

# Access the same 256 bits (C0-C377) as outputs.  Function code=5.
drvModbusTCPAsynConfigure("K1_Cn_Out_Bit",  "Koyo1", 5,  06000, 0400,   0,  1,  "Koyo")

# Access the same 256 bits (C0-C377) as array outputs.  Function code=15.
drvModbusTCPAsynConfigure("K1_Cn_Out_Bit_Array",  "Koyo1", 15,  06000, 0400,   0,   1, "Koyo")

# The DL205 has word access to the Cn bits at Modbus offset 40600 (octal)
# We use the first 16 words (C0-C377) as inputs (256 bits).  Function code=3.
drvModbusTCPAsynConfigure("K1_Cn_In_Word",  "Koyo1", 3, 040600, 020,    0,  100, "Koyo")

# We access the same 16 words (C0-C377) as outputs (256 bits). Function code=6.
drvModbusTCPAsynConfigure("K1_Cn_Out_Word", "Koyo1", 6, 040600, 020,    0,  1,  "Koyo")

# We access the same 16 words (C0-C377) as array outputs (256 bits). Function code=16.
drvModbusTCPAsynConfigure("K1_Cn_Out_Word_Array", "Koyo1", 16, 040600, 020,    0,   1, "Koyo")

# Hex trace format on TCP server
asynSetTraceIOMask("Koyo1",0,4)
# Turn on all debugging on TCP server
#asynSetTraceMask("Koyo1",0,255)

# Hex trace format on modbusTCP server
asynSetTraceIOMask("K1_Yn_In_Bit",0,4)
# Turn on all debugging on modbusTCP server
#asynSetTraceMask("K1_Yn_In_Bit",0,255)
# Dump up to 512 bytes in asynTrace
asynSetTraceIOTruncateSize("K1_Yn_In_Bit",0,512)

dbLoadTemplate("Koyo1.substitutions")

iocInit

