Debug tracing
-------------

One can obtain diagnostic output for a **modbus** port driver using the
"dbior" or "asynPrint" commands at the iocsh or vxWorks shell.
"asynReport" with no arguments will print a brief report for all asyn
drivers, including the drvAsynIPPort or drvAsynSerialPort driver that
**modbus** drivers are connected to, and for all **modbus** port
drivers. For example, a partial output for the Koyo1 application when it
is connected via TCP is:

::

   epics> asynReport
   Koyo1 multiDevice:No canBlock:Yes autoConnect:No
   Port 164.54.160.158:502: Connected
   K1_Xn_Bit multiDevice:Yes canBlock:No autoConnect:Yes
       addr 0 autoConnect Yes enabled Yes connected Yes exceptionActive No
       addr 1 autoConnect Yes enabled Yes connected Yes exceptionActive No
       addr 2 autoConnect Yes enabled Yes connected Yes exceptionActive No
       addr 3 autoConnect Yes enabled Yes connected Yes exceptionActive No
       addr 4 autoConnect Yes enabled Yes connected Yes exceptionActive No
       addr 5 autoConnect Yes enabled Yes connected Yes exceptionActive No
       addr 6 autoConnect Yes enabled Yes connected Yes exceptionActive No
       addr 7 autoConnect Yes enabled Yes connected Yes exceptionActive No
   modbus port: K1_Xn_Bit
   K1_Xn_Word multiDevice:Yes canBlock:No autoConnect:Yes
       addr 0 autoConnect Yes enabled Yes connected Yes exceptionActive No

To obtain more detailed information, one can request information for a
specific **modbus** port driver, and output level >0 as follows:

::

   epics> asynReport 5, "K1_Xn_Word"
   K1_Xn_Word multiDevice:Yes canBlock:No autoConnect:Yes
       enabled:Yes connected:Yes numberConnects 1
       nDevices 1 nQueued 0 blocked:No
       asynManagerLock:No synchronousLock:No
       exceptionActive:No exceptionUsers 0 exceptionNotifys 0
       interfaceList
           asynCommon pinterface 0x4001d180 drvPvt 0x8094f78
           asynDrvUser pinterface 0x4001d10c drvPvt 0x8094f78
           asynUInt32Digital pinterface 0x4001d118 drvPvt 0x8094f78
           asynInt32 pinterface 0x4001d134 drvPvt 0x8094f78
           asynFloat64 pinterface 0x4001d148 drvPvt 0x8094f78
           asynInt32Array pinterface 0x4001d158 drvPvt 0x8094f78
       addr 0 autoConnect Yes enabled Yes connected Yes exceptionActive No
       exceptionActive No exceptionUsers 1 exceptionNotifys 0
       blocked No
   modbus port: K1_Xn_Word
       asyn TCP server:    Koyo1
       modbusFunction:     3
       modbusStartAddress: 040400
       modbusLength:       010
       plcType:            Koyo
       I/O errors:         0
       Read OK:            5728
       Write OK:           0
       pollDelay:          0.100000
       Time for last I/O   3 msec
       Max. I/O time:      12 msec

To obtain run-time debugging output for a driver use the
asynSetTraceMask and asynSetTraceIOMask commands. For example the
following commands will show all I/O to and from the PLC from the
underlying drvAsynIPPort driver:

::

   epics> asynSetTraceIOMask "Koyo1",0,4   # Enable traceIOHex
   epics> asynSetTraceMask "Koyo1",0,9     # Enable traceError and traceIODriver
   epics> 
   2007/04/12 17:27:45.384 164.54.160.158:502 write 12

   00 01 00 00 00 07 ff 02 08 00 00 20 
   2007/04/12 17:27:45.390 164.54.160.158:502 read 13

   00 01 00 00 00 07 ff 02 04 00 00 00 00 
   2007/04/12 17:27:45.424 164.54.160.158:502 write 12

   00 01 00 00 00 07 ff 03 41 00 00 08 
   2007/04/12 17:27:45.432 164.54.160.158:502 read 25

   00 01 00 00 00 13 ff 03 10 00 00 00 00 00 00 00 00 00 00 00 
   00 00 00 00 00 
   ...
   epics> asynSetTraceMask "Koyo1",0,1    # Turn off traceIODriver

The following command shows the I/O from a specific **modbus** port
driver:

::

   epics> asynSetTraceIOMask "K1_Yn_In_Word",0,4   # Enable traceIOHex
   epics> asynSetTraceMask "K1_Yn_In_Word",0,9     # Enable traceError and traceIODriver
   epics> 
   2007/04/12 17:32:31.548 drvModbusAsyn::doModbusIO port K1_Yn_In_Word READ_REGISTERS
   09 00 00 00 00 00 00 00 
   2007/04/12 17:32:31.656 drvModbusAsyn::doModbusIO port K1_Yn_In_Word READ_REGISTERS
   09 00 00 00 00 00 00 00 
   2007/04/12 17:32:31.770 drvModbusAsyn::doModbusIO port K1_Yn_In_Word READ_REGISTERS
   09 00 00 00 00 00 00 00 
   2007/04/12 17:32:31.878 drvModbusAsyn::doModbusIO port K1_Yn_In_Word READ_REGISTERS
   09 00 00 00 00 00 00 00 
   2007/04/12 17:32:31.987 drvModbusAsyn::doModbusIO port K1_Yn_In_Word READ_REGISTERS
   09 00 00 00 00 00 00 00 
   epics> asynSetTraceMask "K1_Yn_In_Word",0,1     # Disable traceIODriver

One can also load an EPICS asyn record on a **modbus** port, and then
use EPICS channel access to turn debugging output on and off. The
following medm screen shows how to turn on I/O tracing using this
method.

asynRecord.adl
~~~~~~~~~~~~~~
Using the asynRecord to turn on traceIODriver and traceIOHex for debugging.

.. figure:: K1_Yn_In_Word.png
    :align: center

The asyn record can also be used to perform actual I/O to the PLC. 
For example the following screen shots shows the asyn record being used to control output Y1 on a PLC. 
Note that the ADDR field is set to 1 (to select Y1) and the data set to 1 (to turn on the output).
Each time the asyn record is processed the value will be sent to the PLC.

.. figure:: K1_Yn_Out_Bit_Asyn.png
    :align: center

asynRegister.adl
~~~~~~~~~~~~~~~~
Using the asynRecord to perform actual I/O to a PLC. 
Note that Interface (IFACE)=asynUInt32Digital, Transfer (TMOD)=Write, and Output (UI32OUT)=1. 
This value will be written to the Y1 output when the record is processed.

.. figure:: K1_Yn_Out_Bit_AsynRegister.png
    :align: center