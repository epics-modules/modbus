Example Applications
--------------------

**modbus** builds an example application called modbusApp. This
application can be run to control any number of Modbus PLCs.

In the iocBoot/iocTest directory there are several startup scripts for
EPICS IOCs. These are designed to test most of the features of the
**modbus** driver on Koyo PLCs, such as the DL series from Automation
Direct.

-  Koyo1.cmd creates **modbus** port drivers to read the X inputs, write
   to the Y outputs, and read and write from the C control registers.
   Each of these sets of inputs and outputs is accessed both as coils
   and as registers (V memory). bi/bo, mbbiDirect/mbboDirect, and
   waveform records are loaded to read and write using these drivers.

-  Koyo2.cmd creates **modbus** port drivers to read the X inputs, write
   to the Y outputs, and read and write from the C control registers.
   Only coil access is used. This example also reads a 4-channel 13-bit
   bipolar A/D converter. This has been tested using both signed-BCD and
   sign plus magnitude binary formats. Note that a ladder logic program
   must be loaded that does the appropriate conversion of the A/D values
   into V memory.

-  st.cmd is a simple example startup script to be run on non-vxWorks
   IOCs. It just loads Koyo1.cmd and Koyo2.cmd. It is invoked using a
   command like:

   ::

            ../../bin/linux-x86/modbusApp st.cmd
            

   One can also load Koyo1.cmd or Koyo2.cmd separately as in:

   ::

            ../../bin/linux-x86/modbusApp Koyo1.cmd
            

   st.cmd.vxWorks is a simple example startup script to be run on
   vxWorks IOCs. It just loads Koyo1.cmd and Koyo2.cmd.

The following is the beginning of Koyo1.cmd when it is configured for
serial RTU with slave address 1 on /dev/ttyS1. It also shows how to
configure TCP and serial ASCII connections. (Koyo PLCs do not support
ASCII however).

::

    # Koyo1.cmd
    
    < envPaths
    
    dbLoadDatabase("../../dbd/modbusApp.dbd")
    modbusApp_registerRecordDeviceDriver(pdbbase)
    
    # Use the following commands for TCP/IP
    #drvAsynIPPortConfigure(const char *portName, 
    #                       const char *hostInfo,
    #                       unsigned int priority, 
    #                       int noAutoConnect,
    #                       int noProcessEos);
    drvAsynIPPortConfigure("Koyo1","164.54.160.158:502",0,0,0)
    asynSetOption("Koyo1",0, "disconnectOnReadTimeout", "Y")
    #modbusInterposeConfig(const char *portName, 
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
    

Note that this example is designed for testing and demonstration
purposes, not as a realistic example of how **modbus** would normally be
used. For example, it loads 6 drivers to access the C control relays
using function codes 1 (read coils), 3 (read holding registers), 5
(write single coil), 6 (write single holding register), 15 (write
multiple coils), and 16 (write multiple holding registers). This allows
for testing of all function codes and record types, including waveforms.
In practice one would normally only load at most 2 drivers for the C
control relays, for example function code 1 (read coils), and function
code 5 (write single coil).

testDataTypes.cmd and testDataTypes.substitutions are used for testing the
different Modbus data types. 
The files ModbusF1_A0_128bits.mbs, ModbusF3_A200_80words.mbs, ModbusF3_A200_80words.mbs,
and ModbusF3_A300_80words.mbs are configuration files for
the `Modbus Slave <http://www.modbustools.com/modbus_slave.asp>`__
program, which is an inexpensive Modbus slave emulator.
This test writes and reads each of the supported Modbus numerical data types as follows:

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - asyn interface
    - Output record
    - Input record
    - Modbus start address
    - Slave simulator file
  * - asynInt32
    - longout
    - longin
    - 100
    - ModbusF3_A100_80words.mbs
  * - asynInt64
    - int64out
    - int64in
    - 200
    - ModbusF3_A200_80words.mbs
  * - asynFloat64
    - ao
    - ai
    - 300
    - ModbusF3_A300_80words.mbs


There is another test application called testClient.cpp which
demonstrates how to instantiate a drvModbusAsyn object and use it to
perform Modbus I/O to an external device. This example is a pure C++
application running without an IOC. The same code could be used in a
driver in an IOC.
