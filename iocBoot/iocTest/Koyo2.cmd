# st.cmd for modbusTCP

dbLoadDatabase("../../dbd/modbusTCP.dbd")
modbusTCP_registerRecordDeviceDriver(pdbbase)

drvAsynIPPortConfigure("Koyo2","164.54.160.158:502",0,0,1)


# NOTE: We use octal numbers for the start address and length (leading zeros)
#       to be consistent with the PLC nomenclature.  This is optional, decimal
#       numbers (no leading zero) or hex numbers can also be used.

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Read 32 bits (Y0-Y37).  Function code=1.
drvModbusTCPAsynConfigure("K2_Yn_In_Bit",       "Koyo2",    1,  04000,  040,    0,  100,    "Koyo")

# The DL205 has bit access to the Yn outputs at Modbus offset 4000 (octal)
# Write 32 bits (Y0-Y37).  Function code=5.
drvModbusTCPAsynConfigure("K2_Yn_Out_Bit",      "Koyo2",    5,  04000,  040,    0,  1,      "Koyo")

# The DL205 has bit access to the Cn bits at Modbus offset 6000 (octal)
# Access 256 bits (C0-C377) as inputs.  Function code=1.
drvModbusTCPAsynConfigure("K2_Cn_In_Bit",       "Koyo2",    1,  06000,  0400,   0,  100,    "Koyo")

# Access the same 256 bits (C0-C377) as outputs.  Function code=5.
drvModbusTCPAsynConfigure("K2_Cn_Out_Bit",      "Koyo2",    5,  06000,  0400,   0,  1,      "Koyo")

# The DL205 has word access to the V3000 memory at Modbus offset 3000 (octal)
# Access 32 words (V3000-V3040) as inputs.  Function code=3, data type BCD.
drvModbusTCPAsynConfigure("K2_V3000_In_Word",   "Koyo2",    3,  03000,  040,    1,  100,    "Koyo")

# Hex trace format on TCP server
asynSetTraceIOMask("Koyo2",0,4)
# Turn on all debugging on TCP server
#asynSetTraceMask("Koyo2",0,255)

# Hex trace format on modbusTCP server
asynSetTraceIOMask("K2_V3000_In_Word",0,4)
# Turn on all debugging on TCP server
#asynSetTraceMask("K2_V3000_In_Word",0,255)

dbLoadTemplate("Koyo2.substitutions")

iocInit

