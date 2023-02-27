EPICS device support
--------------------

**modbus** implements the following standard asyn interfaces:

- asynUInt32Digital
- asynInt32
- asynInt32Array
- asynInt64
- asynFloat64
- asynOctet
- asynCommon
- asynDrvUser

Because it implements these standard interfaces, EPICS device support is
done entirely with the generic EPICS device support provided with asyn
itself. There is no special device support provided as part of
**modbus**.

It is necessary to use asyn R4-8 or later, because some minor
enhancements were made to asyn to support the features required by
**modbus**.

The following tables document the asyn interfaces used by the EPICS
device support.

The **drvUser** parameter is used by the driver to determine what
command is being sent from device support. The default is MODBUS_DATA,
which is thus optional in the link specification in device support. If
no **drvUser** field is specified, or if MODBUS_DATA is specified, then
the Modbus data type for records using the asynInt32, asynInt64, and asynFloat64
interfaces is the default data type specified in the
drvModbusAsynConfigure command. Records can override the default Modbus
data type by specifying datatype-specific **drvUser** field, e.g.
BCD_SIGNED, INT16, FLOAT32_LE, etc.

The **offset** parameter is used to specify the location of the data for
a record relative to the starting Modbus address for that driver. This
**offset** is specified in bits for drivers using Modbus functions 1, 2,
5, and 15 that control discrete inputs or coils. For example, if the
Modbus function is 2 and the Modbus starting address is 04000, then
**offset=2** refers to address 04002. For a Koyo PLC the X inputs are at
this Modbus starting address for Modbus function 2, so **offset=2** is
input X2.

If absolute addressing is being used then the **offset** parameter is an
absolute 16-bit Modbus address, and is not relative to the starting
Modbus address, which is -1.

The **offset** is specified in words for drivers using Modbus functions
3, 4, 6 and 16 that address input registers or holding registers. For
example, if the Modbus function is set to 6 and the Modbus address is
040600 then **offset=2** refers to address 040602. For a Koyo PLC the C
control relays are accessed as 16-bit words at this Modbus starting
address for Modbus function 6. **offset=2** will thus write to the third
16 bit-word, which is coils C40-C57.

For 32-bit or 64-bit data types (INT32_LE, INT32_BE, FLOAT32_LE,
FLOAT32_BE) the **offset** specifies the location of the first 16-bit
register, and the second register is at **offset+1**, etc.

For string data types (STRING_HIGH, STRING_LOW, STRING_HIGH_LOW,
STRING_LOW_HIGH, ZSTRING_HIGH, ZSTRING_LOW, ZSTRING_HIGH_LOW,
ZSTRING_LOW_HIGH) the **offset** specifies the location of the first
16-bit register, and the second register is at **offset+1**, etc.

asynUInt32Digital
~~~~~~~~~~~~~~~~~

asynUInt32Digital device support is selected with

::

   field(DTYP,"asynUInt32Digital")
   field(INP,"@asynMask(portName,offset,mask,timeout)drvUser")

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Modbus function
    - Offset type
    - Data type
    - drvUser
    - Records supported
    - Description
  * - 1, 2
    - Bit
    - Single bit
    - MODBUS_DATA
    - bi, mbbi, mbbiDirect, longin
    - value = (Modbus data & mask), (normally mask=1)
  * - 3, 4, 23
    - 16-bit word
    - 16-bit word
    - MODBUS_DATA
    - bi, mbbi, mbbiDirect, longin
    - value = (Modbus data & mask), (mask selects bits of interest)
  * - 5
    - Bit
    - Single bit
    - MODBUS_DATA
    - bo, mbbo, mbboDirect, longout
    - Modbus write (value & mask), (normally mask=1)
  * - 6, 16
    - 16-bit word
    - 16-bit word
    - MODBUS_DATA
    - bo, mbbo, mbboDirect, longout
    - If mask==0 or mask==0xFFFF does Modbus write (value). 
      Else does read/modify/write:Sets bits that are set in value and set in mask.
      Clears bits that are clear in value and set in mask.
  * - Any
    - NA
    - NA
    - ENABLE_HISTOGRAM
    - bi, mbbi, mbbiDirect, longin
    - Returns 0/1 if I/O time histogramming is disabled/enabled in driver.
  * - Any
    - NA
    - NA
    - ENABLE_HISTOGRAM
    - bo, mbbo, mbboDirect, longout
    - If value = 0/1 then disable/enable I/O time histogramming in driver.

asynInt32
~~~~~~~~~

asynInt32 device support is selected with

::

   field(DTYP,"asynInt32")
   field(INP,"@asyn(portName,offset,timeout)drvUser")
       

or

::

   field(INP,"@asynMask(portName,offset,nbits,timeout)drvUser")
       

The asynMask syntax is used for analog I/O devices, in order to specify
the number of bits in the device. This is required for Modbus because
the driver only knows that it is returning a 16-bit register, but not
the actual number of bits in the device, and hence cannot return
meaningful data with asynInt32->getBounds().

nbits>0 for a unipolar device. For example, nbits=12 means unipolar
12-bit device, with a range of 0 to 4095. nbits<0 for a bipolar device.
For example, nbits=-12 means bipolar 12-bit device, with a range of
-2048 to 2047)

Note: when writing 32-bit or 64-bit values function code 16 should be
used if the device supports it. The write will then be "atomic". If
function code 6 is used then the data will be written in multiple
messages, and there will be an short time period in which the device has
incorrect data.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Modbus function
    - Offset type
    - Data type
    - drvUser
    - Records supported
    - Description
  * - 1, 2
    - Bit
    - Single bit
    - MODBUS_DATA
    - ai, bi, mbbi, longin
    - value = (epicsUInt32)Modbus data
  * - 3, 4, 23
    - 16-bit words
    - 16, 32, or 64-bit word
    - MODBUS_DATA (or datatype-specific value)
    - ai, mbbi, longin
    - value = (epicsInt32)Modbus data
  * - 5
    - Bit
    - Single bit
    - MODBUS_DATA
    - ao, bo, mbbo, longout
    - Modbus write value
  * - 6, 16, 23
    - 16-bit words
    - 16, 32, or 64-bit word
    - MODBUS_DATA (or datatype-specific value)
    - ao, mbbo, longout
    - Modbus write value
  * - Any
    - NA
    - NA
    - MODBUS_READ
    - ao, bo, longout
    - Writing to a Modbus input driver with this drvUser value will force the poller thread
      to run once immediately, regardless of the value of POLL_DELAY.
  * - Any
    - NA
    - NA
    - READ_OK
    - ai, longin
    - Returns number of successful read operations on this asyn port
  * - Any
    - NA
    - NA
    - WRITE_OK
    - ai, longin
    - Returns number of successful write operations on this asyn port
  * - Any
    - NA
    - NA
    - IO_ERRORS
    - ai, longin
    - Returns number of I/O errors on this asyn port
  * - Any
    - NA
    - NA
    - LAST_IO_TIME
    - ai, longin
    - Returns number of milliseconds for last I/O operation
  * - Any
    - NA
    - NA
    - MAX_IO_TIME
    - ai, longin
    - Returns maximum number of milliseconds for I/O operations
  * - Any
    - NA
    - NA
    - HISTOGRAM_BIN_TIME
    - ao, longout
    - Sets the time per bin in msec in the statistics histogram

asynInt64
~~~~~~~~~

asynInt64 device support is selected with

::

   field(DTYP,"asynInt64")
   field(INP,"@asyn(portName,offset,timeout)drvUser")
       
Note: when writing 32-bit or 64-bit values function code 16 should be
used if the device supports it. The write will then be "atomic". If
function code 6 is used then the data will be written in multiple
messages, and there will be an short time period in which the device has
incorrect data.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Modbus function
    - Offset type
    - Data type
    - drvUser
    - Records supported
    - Description
  * - 1, 2
    - Bit
    - Single bit
    - MODBUS_DATA
    - ai, longin, int64in
    - value = (epicsUInt64)Modbus data
  * - 3, 4, 23
    - 16-bit words
    - 16, 32, or 64-bit word
    - MODBUS_DATA (or datatype-specific value)
    - ai, longin, int64in
    - value = (epicsInt64)Modbus data
  * - 5
    - Bit
    - Single bit
    - MODBUS_DATA
    - ao, longout, int64out
    - Modbus write value
  * - 6, 16, 23
    - 16-bit words
    - 16, 32, or 64-bit word
    - MODBUS_DATA (or datatype-specific value)
    - ao, longout, int64out
    - Modbus write value

asynFloat64
~~~~~~~~~~~

asynFloat64 device support is selected with

::

   field(DTYP,"asynFloat64")
   field(INP,"@asyn(portName,offset,timeout)drvUser")

Note: when writing 32-bit or 64-bit values function code 16 should be
used if the device supports it. The write will then be "atomic". If
function code 6 is used then the data will be written in multiple
messages, and there will be an short time period in which the device has
incorrect data.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Modbus function
    - Offset type
    - Data type
    - drvUser
    - Records supported
    - Description
  * - 1, 2
    - Bit
    - Single bit
    - MODBUS_DATA
    - ai
    - value = (epicsFloat64)Modbus data
  * - 3, 4, 23
    - 16-bit words
    - 16, 32, or 64-bit word
    - MODBUS_DATA (or datatype-specific value)
    - ai
    - value = (epicsFloat64)Modbus data
  * - 5
    - Bit
    - Single bit
    - MODBUS_DATA
    - ao
    - Modbus write (epicsUInt16)value
  * - 6, 16, 23
    - 16-bit word
    - 16-bit word
    - MODBUS_DATA (or datatype-specific value)
    - ao
    - Modbus write value
  * - Any
    - NA
    - NA
    - POLL_DELAY
    - ai, ao
    - Read or write the delay time in seconds between polls for the read poller thread.
      If <=0 then the poller thread does not run periodically, it only runs when it
      is woken up by an epicsEvent signal, which happens when the driver has an asynInt32
      write with the MODBUS_READ drvUser string.

asynInt32Array
~~~~~~~~~~~~~~

asynInt32Array device support is selected with

::

   field(DTYP,"asynInt32ArrayIn")
   field(INP,"@asyn(portName,offset,timeout)drvUser")
       

or

::

   field(DTYP,"asynInt32ArrayOut")
   field(INP,"@asyn(portName,offset,timeout)drvUser")
       

asynInt32Array device support is used to read or write arrays of up to
2000 coil values or up to 125 16-bit registers. It is also used to read
the histogram array of I/O times when histogramming is enabled.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Modbus function
    - Offset type
    - Data type
    - drvUser
    - Records supported
    - Description
  * - 1, 2
    - Bit
    - Array of bits
    - MODBUS_DATA
    - waveform (input)
    - value = (epicsInt32)Modbus data[]
  * - 3, 4, 23
    - 16-bit word
    - Array of 16, 32 or 64-bit words
    - MODBUS_DATA (or datatype-specific value)
    - waveform (input)
    - value = (epicsInt32)Modbus data[]
  * - 15
    - Bit
    - Array of bits
    - MODBUS_DATA
    - waveform (output)
    - Modbus write (epicsUInt16)value[]
  * - 16, 23
    - 16-bit word
    - Array of 16, 32, or 64-bit words
    - MODBUS_DATA (or datatype-specific value)
    - waveform (output)
    - Modbus write value[]
  * - Any
    - 32-bit word
    - NA
    - READ_HISTOGRAM
    - waveform (input)
    - Returns a histogram array of the I/O times in milliseconds since histogramming was
      last enabled.
  * - Any
    - 32-bit word
    - NA
    - HISTOGRAM_TIME_AXIS
    - waveform (input)
    - Returns the time axis of the histogram data. Each element is HISTOGRAM_BIN_TIME
      msec.

asynOctet
~~~~~~~~~

asynOctet device support is selected with

::

   field(DTYP,"asynOctetRead")
   field(INP,"@asyn(portName,offset,timeout)drvUser[=number_of_characters]")
       
or

::

   field(DTYP,"asynOctetWrite")
   field(INP,"@asyn(portName,offset,timeout)drvUser[=number_of_characters]")
       
asynOctet device support is used to read or write strings of up to 250
characters.

Note: The 0 terminating byte at the end of the string in a waveform
record or stringout record is only written to the Modbus device
if one of the ZSTRING_* drvUser types is used.

Note: On input the number of characters read from the Modbus device will be the lesser of:

- The number of characters in the record minus the terminating 0 byte
  (39 for stringin, NELM-1 for waveform) or
- The number of characters specified after drvUser (minus the
  terminating 0 byte) or
- The number of characters contained in the registers defined
  modbusLength argument to drvModbusAsynConfigure (modbusLength or
  modbusLength*2 depending on whether the drvUser field specifies 1 or 2
  characters per register.

The string will be truncated if any of the characters read from Modbus
is a 0 byte, but there is no guarantee that the last character in the
string is followed by a 0 byte in the Modbus registers. Generally either
number_of_characters or NELM in the waveform record should be used to
define the correct length for the string.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Modbus function
    - Offset type
    - Data type
    - drvUser
    - Records supported
    - Description
  * - 3, 4, 23
    - 16-bit word
    - String of characters
    - STRING_HIGH, STRING_LOW, STRING_HIGH_LOW, or STRING_LOW_HIGH</br>
      ZSTRING_HIGH, ZSTRING_LOW, ZSTRING_HIGH_LOW, or ZSTRING_LOW_HIGH
    - waveform (input) or stringin
    - value = Modbus data[]
  * - 16, 23
    - 16-bit word
    - String of characters
    - STRING_HIGH, STRING_LOW, STRING_HIGH_LOW, or STRING_LOW_HIGH</br>
      ZSTRING_HIGH, ZSTRING_LOW, ZSTRING_HIGH_LOW, or ZSTRING_LOW_HIGH
    - waveform (output) or stringout
    - Modbus write value[]

Template files
~~~~~~~~~~~~~~

**modbus** provides example template files in the modbusApp/Db
directory. These include the following.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Files
    - Description
    - Macro arguments
  * - bi_bit.template
    - asynUInt32Digital support for bi record with discrete inputs or coils. Mask=1.
    - P, R, PORT, OFFSET, ZNAM, ONAM, ZSV, OSV, SCAN
  * - bo_bit.template
    - asynUInt32Digital support for bo record with coil outputs. Mask=1.
    - P, R, PORT, OFFSET, ZNAM, ONAM
  * - bi_word.template
    - asynUInt32Digital support for bi record with register inputs.
    - P, R, PORT, OFFSET, MASK, ZNAM, ONAM, ZSV, OSV, SCAN
  * - bo_word.template
    - asynUInt32Digital support for bo record with register outputs.
    - P, R, PORT, OFFSET, MASK, ZNAM, ONAM
  * - mbbiDirect.template
    - asynUInt32Digital support for mbbiDirect record with register inputs.
    - P, R, PORT, OFFSET, MASK, SCAN
  * - mbboDirect.template
    - asynUInt32Digital support for mbboDirect record with register outputs.
    - P, R, PORT, OFFSET, MASK
  * - longin.template
    - asynUInt32Digital support for longin record with register inputs. Mask=0xFFFF.
    - P, R, PORT, OFFSET, SCAN
  * - longout.template
    - asynUInt32Digital support for longout record with register outputs. Mask=0xFFFF.
    - P, R, PORT, OFFSET
  * - longinInt32.template
    - asynInt32 support for longin record with register inputs.
    - P, R, PORT, OFFSET, SCAN, DATA_TYPE
  * - longoutInt32.template
    - asynInt32 support for longout record with register outputs.
    - P, R, PORT, OFFSET, DATA_TYPE
  * - ai.template
    - asynInt32 support for ai record with LINEAR conversion
    - P, R, PORT, OFFSET, BITS, EGUL, EGUF, PREC, SCAN
  * - ao.template
    - asynInt32 support for ao record with LINEAR conversion
    - P, R, PORT, OFFSET, BITS, EGUL, EGUF, PREC
  * - ai_average.template
    - asynInt32Average support for ai record with LINEAR conversion. This support gets
      callbacks each time the poll thread reads the analog input, and averages readings
      until the record is processed.
    - P, R, PORT, OFFSET, BITS, EGUL, EGUF, PREC, SCAN
  * - intarray_in.template
    - asynInt32Array support for waveform record with discrete, coil, or register inputs.
    - P, R, PORT, OFFSET, NELM, SCAN
  * - intarray_out.template
    - asynInt32Array support for waveform record with discrete, coil, or register outputs.
    - P, R, PORT, OFFSET, NELM
  * - int64in.template
    - asynInt64 support for int64in record with register inputs.
    - P, R, PORT, OFFSET, SCAN, DATA_TYPE
  * - int64out.template
    - asynInt64 support for int64out record with register outputs.
    - P, R, PORT, OFFSET, DATA_TYPE
  * - aiFloat64.template
    - asynFloat64 support for ai record
    - P, R, PORT, OFFSET, LOPR, HOPR, PREC, SCAN, DATA_TYPE
  * - aoFloat64.template
    - asynFloat64 support for ao record
    - P, R, PORT, OFFSET, LOPR, HOPR, PREC, DATA_TYPE
  * - stringin.template
    - asynOctet support for stringin record
    - P, R, PORT, OFFSET, DATA_TYPE, SCAN
  * - stringout.template
    - asynOctet support for stringout record
    - P, R, PORT, OFFSET, DATA_TYPE, INITIAL_READBACK
  * - stringWaveformIn.template
    - asynOctet input support for waveform record
    - P, R, PORT, OFFSET, DATA_TYPE, NELM, SCAN
  * - stringWaveformOut.template
    - asynOctet output support for waveform record
    - P, R, PORT, OFFSET, DATA_TYPE, NELM, INITIAL_READBACK
  * - asynRecord.template
    - Support for asyn record. Useful for controlling trace printing, and for debugging.
    - P, R, PORT, ADDR, TMOD, IFACE
  * - poll_delay.template
    - Support for ao record to control the delay time for the poller thread.
    - P, R, PORT
  * - poll_trigger.template
    - Support for bo record to trigger running the poller thread.
    - P, R, PORT
  * - statistics.template
    - Support for bo, longin and waveform records to read I/O statistics for the port.
    - P, R, PORT, SCAN

The following table explains the macro parameters used in the preceding table.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Macro
    - Description
  * - P
    - Prefix for record name. Complete record name is $(P)$(R).
  * - R
    - Record name. Complete record name is $(P)$(R).
  * - PORT
    - Port name for **modbus** asyn port.
  * - OFFSET
    - Offset for Modbus data relative to start address for this port.
  * - MASK
    - Bit mask used to select data for this record.
  * - ZNAM
    - String for 0 value for bi/bo records.
  * - ONAM
    - String for 1 value for bi/bo records.
  * - ZSV
    - 0 severity for bi/bo records.
  * - OSV
    - 1 severity for bi/bo records.
  * - BITS
    - Number of bits for analog I/O devices. >0=unipolar, <0=bipolar.
  * - DATA_TYPE
    - drvUser field specifying the Modbus data type. If this field is blank or is MODBUS_DATA
      then the default datatype specified in the drvModbusAsynConfigure command is used.
      Other allowed values are listed in the table above (UINT16, INT16SM, BCD_SIGNED,
      etc.)
  * - EGUL
    - Engineering value for lower limit of analog device.
  * - EGUF
    - Engineering value for upper limit of analog device.
  * - LOPR
    - Lower display limit of analog device.
  * - HOPR
    - Upper display limit of analog device.
  * - PREC
    - Number of digits of precision for ai/ao records.
  * - NELM
    - Number of elements in waveform records.
  * - ADDR
    - Address for asyn record, same as OFFSET above.
  * - TMOD
    - Transfer mode for asyn record.
  * - IFACE
    - asyn interface for asyn record.
  * - SCAN
    - Scan rate for record (e.g. "1 second", "I/O Intr", etc.).
  * - INITIAL_READBACK
    - Controls whether an initial readback from the device is done for the stringout or
      string waveform output records.
