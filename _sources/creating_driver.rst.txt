Creating a **modbus** port driver
---------------------------------

Before **modbus** port drivers can be created, it is necessary to first
create at least one asyn TCP/IP, UDP/IP or serial port driver to communicate
with the hardware. The commands required depend on the communications
link being used.

TCP/IP UDP/IP
~~~~~~~~~~~~~

For TCP/IP or UDP/IP use the following standard asyn command:

::

   drvAsynIPPortConfigure(portName, hostInfo, priority, noAutoConnect, noProcessEos)

Documentation on this command can be found in the `asynDriver
documentation <https://epics-modules.github.io/master/asyn/R4-40/asynDriver.html#drvAsynIPPort>`__.

The following example creates an asyn IP port driver called "Koyo1" on
port 502 at IP address 164.54.160.158. The default priority is used and
the noAutoConnect flag is set to 0 so that asynManager will do normal
automatic connection management. Note that the noProcessEos flag is set to 0
so it is using the asynInterposeEos interface.  
The asynInterposeEos interface handles end-of-string (EOS) processing, which is not needed for Modbus TCP.
However, it also handles issuing repeated read requests until the requested number of bytes
has been received, which the low-level asyn IP port driver does not do.  
Normally Modbus TCP sends responses in a single packet, so this may not be needed, but using 
the asynInterpose interface does no harm.
However, the asynInterposeEos interface is definitely needed when using drvAsynIPPortConfigure to talk 
to a terminal server that is communicating with the Modbus device over Modbus RTU or ASCII, 
because then the communication from the device may well be broken up into multiple packets.
To use UDP rather than TCP, add " UDP" after the host name/number and optional port number.

::

   drvAsynIPPortConfigure("Koyo1","164.54.160.158:502",0,0,0)

Serial RTU
~~~~~~~~~~

For serial RTU use the following standard asyn commands
This is recommended even when using actual:

::

   drvAsynSerialPortConfigure(portName, ttyName, priority, noAutoConnect, noProcessEos)
   asynSetOption(portName, addr, key, value)

Documentation on these commands can be found in the `asynDriver
documentation <https://epics-modules.github.io/master/asyn/R4-40/asynDriver.html#drvAsynSerialPort>`__.

The following example creates an asyn local serial port driver called
"Koyo1" on /dev/ttyS1. The default priority is used and the
noAutoConnect flag is set to 0 so that asynManager will do normal
automatic connection management. The noProcessEos flag is set to 0
because Modbus over serial requires end-of-string processing. The serial
port parameters are configured to 38400 baud, no parity, 8 data bits, 1
stop bit.

::

   drvAsynSerialPortConfigure("Koyo1", "/dev/ttyS1", 0, 0, 0)
   asynSetOption("Koyo1",0,"baud","38400")
   asynSetOption("Koyo1",0,"parity","none")
   asynSetOption("Koyo1",0,"bits","8")
   asynSetOption("Koyo1",0,"stop","1")

Serial ASCII
~~~~~~~~~~~~

For serial ASCII use the same commands described above for serial RTU.
After the asynSetOption commands use the following standard asyn
commands:

::

   asynOctetSetOutputEos(portName, addr, eos)
   asynOctetSetInputEos(portName, addr, eos)

Documentation on these commands can be found in the `asynDriver
documentation <https://epics-modules.github.io/master/asyn/R4-41/asynDriver.html#DiagnosticAids>`__.

The following example creates an asyn local serial port driver called
"Koyo1" on /dev/ttyS1. The default priority is used and the
noAutoConnect flag is set to 0 so that asynManager will do normal
automatic connection management. The noProcessEos flag is set to 0
because Modbus over serial requires end-of-string processing. The serial
port parameters are configured to 38400 baud, no parity, 8 data bits, 1
stop bit. The input and output end-of-string is set to CR/LF.

::

   drvAsynSerialPortConfigure("Koyo1", "/dev/ttyS1", 0, 0, 0)
   asynSetOption("Koyo1",0,"baud","38400")
   asynSetOption("Koyo1",0,"parity","none")
   asynSetOption("Koyo1",0,"bits","8")
   asynSetOption("Koyo1",0,"stop","1")
   asynOctetSetOutputEos("Koyo1",0,"\r\n")
   asynOctetSetInputEos("Koyo1",0,"\r\n")

modbusInterposeConfig
~~~~~~~~~~~~~~~~~~~~~

After creating the asynIPPort or asynSerialPort driver, the next step is
to add the asyn "interpose interface" driver. This driver takes the
device-independent Modbus frames and adds or removes the
communication-link specific information for the TCP, UDP, RTU, or ASCII link
protocols. The interpose driver is created with the command:

::

   modbusInterposeConfig(portName, 
                         linkType,
                         timeoutMsec,
                         writeDelayMsec)

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Parameter
    - Data type
    - Description
  * - portName
    - string
    - Name of the asynIPPort or asynSerialPort previously created.
  * - linkType
    - int
    - Modbus link layer type:, 0 = TCP/IP, 1 = RTU, 2 = ASCII, 3 = UDP/IP
  * - timeoutMsec
    - int
    - The timeout in milliseconds for write and read operations to the underlying asynOctet
      driver. This value is used in place of the timeout parameter specified in EPICS
      device support. If zero is specified then a default timeout of 2000 milliseconds
      is used.
  * - writeDelayMsec
    - int
    - The delay in milliseconds before each write from EPICS to the device. This is typically
      only needed for Serial RTU devices. The Modicon Modbus Protocol Reference Guide
      says this must be at least 3.5 character times, e.g. about 3.5ms at 9600 baud, for
      Serial RTU. The default is 0.
      
For the serial ASCII example above, after the asynOctetSetInputEos
command, the following command would be used. This uses a timeout of 1
second, and a write delay of 0 ms.

::

   modbusInterposeConfig("Koyo1",2,1000,0)

drvModbusAsynConfigure
~~~~~~~~~~~~~~~~~~~~~~

Once the asyn IP or serial port driver has been created, and the
modbusInterpose driver has been configured, a **modbus** port driver is
created with the following command:

::

   drvModbusAsynConfigure(portName, 
                          tcpPortName,
                          slaveAddress, 
                          modbusFunction, 
                          modbusStartAddress, 
                          modbusLength,
                          dataType,
                          pollMsec, 
                          plcType);

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Parameter
    - Data type
    - Description
  * - portName
    - string
    - Name of the **modbus** port to be created.
  * - tcpPortName
    - string
    - Name of the asyn IP or serial port previously created.
  * - slaveAddress
    - int
    - The address of the Modbus slave. This must match the configuration of the Modbus
      slave (PLC) for RTU and ASCII. For TCP or UDP the slave address is used for the "unit identifier",
      the last field in the MBAP header. The "unit identifier" is ignored by most PLCs,
      but may be required by some.
  * - modbusFunction
    - int
    - Modbus function code (1, 2, 3, 4, 5, 6, 15, 16, 17, 123 (for 23 read-only), or 223 (for
      23 write-only)).
  * - modbusStartAddress
    - int
    - Start address for the Modbus data segment to be accessed. For relative addressing
      this must be in the range 0-65535 decimal, or 0-0177777 octal. For absolute addressing
      this must be set to -1.
  * - modbusLength
    - int
    - The length of the Modbus data segment to be accessed. 
      This is specified in bits for Modbus functions 1, 2, 5 and 15.
      It is specified in 16-bit words for Modbus functions 3, 4, 6, 16, 17, or 23.
      Length limit is 2000 for functions 1 and 2, 1968 for functions 5 and 15, 125 for functions 3 and 4, 
      and 123 for functions 6, 16, 17, and 23.
      For absolute addressing this must be set to the size of required by the largest
      single Modbus operation that may be used. This would be 1 if all Modbus reads and
      writes are for 16-bit registers, but it would be 4 if 64-bit floats (4 16-bit registers)
      are being used, and 100 (for example) if an Int32 waveform record with NELM=50
      is being read or written.
  * - modbusDataType
    - string
    - This sets the default data type for this port. This is the data type used if the
      drvUser field of a record is empty, or if it is MODBUS_DATA. The supported Modbus
      data type strings are listed in the table below. This argument can either be one of the
      strings shown in the table below, and defined in `drvModbusAsyn.h`, or it can be the
      numeric `modbusDataType_t` enum also defined in `drvModbusAsyn.h`.  The enum values
      are less convenient and understandable then the string equivalents. 
      NOTE: the enum values changed between R3-0 and R3-1, which may require changes
      to startup scripts.  INT16 and UINT16 were swapped and everything beyond
      INT32_LE is different.
  * - pollMsec
    - int
    - Polling delay time in msec for the polling thread for read functions.
      For write functions, a non-zero value means that the Modbus data should, be read once when the port driver is first created.
  * - plcType
    - string
    - Type of PLC (e.g. Koyo, Modicon, etc.).
      This parameter is currently used to print information in asynReport.
      It is also used to treat Wago devices specially if the plcType string contains the
      substring "Wago". See the note below.

Modbus register data types
~~~~~~~~~~~~~~~~~~~~~~~~~~

Modbus function codes 3, 4, 6, and 16 are used to access 16-bit
registers. The Modbus specification does not define how the data in
these registers is to be interpreted, for example as signed or unsigned
numbers, binary coded decimal (BCD) values, etc. In fact many
manufacturers combine multiple 16-bit registers to encode 32-bit
integers, 32-bit or 64-bit floats, etc. The following table lists the
data types supported by **modbus**. The default data type for the port
is defined with the modbusDataType parameter described above. The data
type for particular record can override the default by specifying a
different data type with the drvUser field in the link. The driver uses
this information to convert the number between EPICS device support and
Modbus. Data is transferred to and from EPICS device support as
epicsUInt32, epicsInt32, epicsInt64, and epicsFloat64 numbers. Note that the data
type conversions described in this table only apply for records using
the asynInt32, asynInt64, or asynFloat64 interfaces, they do not apply when using
the asynUInt32Digital interface. The asynUInt32Digital interface always
treats the registers as unsigned 16-bit integers.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - drvUser field
    - Description
  * - INT16
    - 16-bit signed (2's complement) integers. This data type extends the sign bit when
      converting to epicsInt32.
  * - INT16SM
    - 16-bit binary integers, sign and magnitude format. In this format bit 15 is the
      sign bit, and bits 0-14 are the absolute value of the magnitude of the number. This
      is one of the formats used, for example, by Koyo PLCs for numbers such as ADC conversions.
  * - BCD_UNSIGNED
    - Binary coded decimal (BCD), unsigned. This data type is for a 16-bit number consisting
      of 4 4-bit nibbles, each of which encodes a decimal number from 0-9. A BCD number
      can thus store numbers from 0 to 9999. Many PLCs store some numbers in BCD format.
  * - BCD_SIGNED
    - 4-digit binary coded decimal (BCD), signed. This data type is for a 16-bit number
      consisting of 3 4-bit nibbles, and one 3-bit nibble. Bit 15 is a sign bit. Signed
      BCD numbers can hold values from -7999 to +7999. This is one of the formats used
      by Koyo PLCs for numbers such as ADC conversions.
  * - UINT16
    - Unsigned 16-bit binary integers.
  * - INT32_LE
    - 32-bit integers, little endian (least significant word at Modbus address N, most
      significant word at Modbus address N+1).
  * - INT32_LE_BS
    - 32-bit integers, little endian (least significant word at Modbus address N, most
      significant word at Modbus address N+1).  Bytes within each word are swapped.
  * - INT32_BE
    - 32-bit integers, big endian (most significant word at Modbus address N, least significant
      word at Modbus address N+1).
  * - INT32_BE_BS
    - 32-bit integers, big endian (most significant word at Modbus address N, least significant
      word at Modbus address N+1).   Bytes within each word are swapped.
  * - UINT32_LE
    - Unsigned 32-bit integers, little endian (least significant word at Modbus address N, most
      significant word at Modbus address N+1).
  * - UINT32_LE_BS
    - Unsigned 32-bit integers, little endian (least significant word at Modbus address N, most
      significant word at Modbus address N+1).  Bytes within each word are swapped.
  * - UINT32_BE
    - Unsigned 32-bit integers, big endian (most significant word at Modbus address N, least significant
      word at Modbus address N+1).
  * - UINT32_BE_BS
    - Unsigned 32-bit integers, big endian (most significant word at Modbus address N, least significant
      word at Modbus address N+1).   Bytes within each word are swapped.
  * - INT64_LE
    - 64-bit integers, little endian (least significant word at Modbus address N, most
      significant word at Modbus address N+3).
  * - INT64_LE_BS
    - 64-bit integers, little endian (least significant word at Modbus address N, most
      significant word at Modbus address N+3).  Bytes within each word are swapped.
  * - INT64_BE
    - 64-bit integers, big endian (most significant word at Modbus address N, least significant
      word at Modbus address N+3).
  * - INT64_BE_BS
    - 64-bit integers, big endian (most significant word at Modbus address N, least significant
      word at Modbus address N+3).   Bytes within each word are swapped.
  * - UINT64_LE
    - Unsigned 64-bit integers, little endian (least significant word at Modbus address N, most
      significant word at Modbus address N+3).
  * - UINT64_LE_BS
    - Unsigned 64-bit integers, little endian (least significant word at Modbus address N, most
      significant word at Modbus address N+3).  Bytes within each word are swapped.
  * - UINT64_BE
    - Unsigned 64-bit integers, big endian (most significant word at Modbus address N, least significant
      word at Modbus address N+3).
  * - UINT64_BE_BS
    - Unsigned 64-bit integers, big endian (most significant word at Modbus address N, least significant
      word at Modbus address N+3).   Bytes within each word are swapped.
  * - FLOAT32_LE
    - 32-bit floating point, little endian (least significant word at Modbus address N,
      most significant word at Modbus address N+1).
  * - FLOAT32_LE_BS
    - 32-bit floating point, little endian (least significant word at Modbus address N,
      most significant word at Modbus address N+1). Bytes within each word are swapped.
  * - FLOAT32_BE
    - 32-bit floating point, big endian (most significant word at Modbus address N, least
      significant word at Modbus address N+1).
  * - FLOAT32_BE_BS
    - 32-bit floating point, big endian (most significant word at Modbus address N, least
      significant word at Modbus address N+1). Bytes within each word are swapped.
  * - FLOAT64_LE
    - 64-bit floating point, little endian (least significant word at Modbus address N,
      most significant word at Modbus address N+3).
  * - FLOAT64_LE_BS
    - 64-bit floating point, little endian (least significant word at Modbus address N,
      most significant word at Modbus address N+3). Bytes within each word are swapped.
  * - FLOAT64_BE
    - 64-bit floating point, big endian (most significant word at Modbus address N, least
      significant word at Modbus address N+3).
  * - FLOAT64_BE_BS
    - 64-bit floating point, big endian (most significant word at Modbus address N, least
      significant word at Modbus address N+3). Bytes within each word are swapped.
  * - STRING_HIGH
    - String data. One character is stored in the high byte of each register.
  * - STRING_LOW
    - String data. One character is stored in the low byte of each register.
  * - STRING_HIGH_LOW
    - String data. Two characters are stored in each register, the first in the high byte
      and the second in the low byte.
  * - STRING_LOW_HIGH
    - String data. Two characters are stored in each register, the first in the low byte
      and the second in the high byte.
  * - ZSTRING_HIGH
    - Zero terminated string data. One character is stored in the high byte of each register.
  * - ZSTRING_LOW
    - Zero terminated string data. One character is stored in the low byte of each register.
  * - ZSTRING_HIGH_LOW
    - Zero terminated string data. Two characters are stored in each register, the first in the high byte
      and the second in the low byte.
  * - ZSTRING_LOW_HIGH
    - Zero terminated string data. Two characters are stored in each register, the first in the low byte
      and the second in the high byte.

NOTE: if it is desired to transmit BCD numbers untranslated to EPICS
over the asynInt32 interface, then data type 0 should be used, because
no translation is done in this case. 

NOTE: the ZSTRING_* types are meant for output records. 
For input records they are identical to their STRING_* counterparts.

NOTE: For big-endian formats the _BE format is order in which an IEEE value would
be stored on a big-endian machine, and _BE_BS swaps the bytes in each 16-bit word
relative to IEEE specification.
However, for little-endian formats the _LE format is byte-swapped within each 16-bit word 
compared how the IEEE value would be be stored on a little-endian machine.  
The _LE_BS format is the order in which an IEEE value would be stored on a little-endian machine.
This is done for backwards compatibility, because that is how _LE has always been stored in
previous versions of this modbus module, before the byte-swapped formats were added.

The following is an example ai record using 32-bit floating point
values:

::

   # ai record template for register inputs
   record(ai, "$(P)$(R)") {
       field(DTYP,"asynFloat64")
       field(INP,"@asyn($(PORT) $(OFFSET))FLOAT32_LE")
       field(HOPR,"$(HOPR)")
       field(LOPR,"$(LOPR)")
       field(PREC,"$(PREC)")
       field(SCAN,"$(SCAN)")
   }   

Note for Wago devices
~~~~~~~~~~~~~~~~~~~~~

This initial read operation is normally done at the same Modbus address
as the write operations. However, Wago devices are different from other
Modbus devices because the address to read back a register is not the
same as the address to write the register. For Wago devices the address
used to read back the initial value for a Modbus write function must be
0x200 greater than the address for the write function. This is handled
by adding this 0x200 offset for the readback address if the plcType
argument to drvModbusAsynConfigure contains the substring "Wago" (case
sensitive). Note that this does not affect the address for Wago read
functions. The user must specify the actual Modbus address for read
functions.

Number of drvAsynIPPort drivers for TCP
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each drvAsynIPPort driver creates a separate TCP/IP socket connection to
the PLC. It is possible to have all of the **modbus** port drivers share
a single drvAsynIPPort driver. In this case all I/O to the PLC is done
over a single socket in a "serial" fashion. A transaction for one
**modbus** driver must complete before a transaction for another
**modbus** driver can begin. It is also possible to create multiple
drvAsynIPPort drivers (sockets) to a single PLC and, for example, use a
different drvAsynIPPort for each **modbus** port. In this case I/O
operations from multiple **modbus** drivers can proceed in parallel,
rather than serially. This could improve performance at the expense of
more CPU load on the IOC and PLC, and more network traffic.

It is important to note, however, that many PLCs will time out sockets
after a few seconds of inactivity. This is not a problem with **modbus**
drivers that use read function codes, because they are polling
frequently. But **modbus** drivers that use write function codes may
only do occasional I/O, and hence may time out if they are the only ones
communicating through a drvAsynIPPort driver. Thus, it is usually
necessary for **modbus** drivers with write function codes to use the
same drvAsynIPPort driver (socket) as at least one **modbus** driver
with a read function code to avoid timeouts.

The choice of how many drvAsynIPPort drivers to use per PLC will be
based on empirical performance versus resource usage considerations. In
general it is probably a good idea to start with one drvAsynIPPort
server per PLC (e.g. shared by all **modbus** drivers for that PLC) and
see if this results in satisfactory performance.

Number formats
~~~~~~~~~~~~~~

It can be convenient to specify the modbusStartAddress and modbusLength
in octal, rather than decimal, because this is the convention on most
PLCs. In the iocsh and vxWorks shells this is done by using a leading 0
on the number, i.e. 040400 is an octal number.
