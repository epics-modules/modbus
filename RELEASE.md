# modbus: Modbus Support - Release Notes

## R3-2 (November 21, 2020)
- Changed the dataType argument to drvModusAsynConfigure(). 
  Previously this was the `int` value corresponding to one of the `modbusDataType_t` enums
  defined `drvModbusAsyn.h`.  This is not very convenient, so the dataType argument was changed
  to a string.  It can now either be the enum value (for backwards compatibility) or one of the
  strings like `INT32_LE`.  The string comparison is case-insensitive.
- Improved the drvModbusAsyn::report() function to print the default dataType for the driver.
  The dataType is printed both as the integer enum value and as the corresponding string.
- Changed the documentation and all example IOCs to set noProcessEos=0 in drvAsynIPPortConfigure()
  commands, so that the asynInterposeEos interface is used.  Previously it was 1 in most cases.
  The asynInterposeEos interface handles end-of-string (EOS) processing, which is not needed for Modbus TCP.
  However, it also handles issuing repeated read requests until the requested number of bytes
  has been received, which the low-level asyn IP port driver does not do.  
  Normally Modbus TCP sends responses in a single packet, so this may not be needed, but using 
  the asynInterpose interface does no harm.
  However, the asynInterposeEos interface is definitely needed when using drvAsynIPPortConfigure to talk 
  to a terminal server that is communicating with the Modbus device over Modbus RTU or ASCII, 
  because then the communication from the device may well be broken up into multiple packets.

## R3-1 (July 28, 2020)
- Added support for the asynInt64 interface between device support and the driver.
- Added support for 22 new data types including unsigned integers, 
  64-bit integers, byte-swapped versions of all 32-bit and 64-bit data types,
  and zero-terminated strings.
    - INT32_LE_BS    Byte-swapped Int32 little-endian
    - INT32_BE_BS    Byte-swapped Int32 big-endian
    - UINT32_LE      Unsigned Int32 little-endian
    - UINT32_BE      Unsigned Int32 big-endian
    - UINT32_LE_BS   Byte-swapped unsigned Int32 little-endian
    - UINT32_BE_BS   Byte-swapped unsigned Int32 big-endian
    - INT64_LE       Int64 little-endian
    - INT64_BE       Int64 big-endian
    - INT64_LE_BS    Byte-swapped Int64 little-endian
    - INT64_BE_BS    Byte-swapped Int64 big-endian
    - UINT64_LE      Unsigned Int64 little-endian
    - UINT64_BE      Unsigned Int64 big-endian
    - UINT64_LE_BS   Byte-swapped unsigned Int64 little-endian
    - UINT64_BE_BS   Byte-swapped unsigned Int64 big-endian
    - FLOAT32_LE_BS  Byte-swapped Float32 little-endian
    - FLOAT32_BE_BS  Byte-swapped Float32 big-endian
    - FLOAT64_LE_BS  Byte-swapped Float64 little-endian
    - FLOAT64_BE_BS  Byte-swapped Float64 big-endian
    - ZSTRING_HIGH   Zero terminated string data. One character is stored in the high byte of each register.
    - ZSTRING_LOW    Zero terminated string data. One character is stored in the low byte of each register.
    - ZSTRING_HIGH_LOW Zero terminated string data. Two characters are stored in each register, 
      the first in the high byte and the second in the low byte.
    - ZSTRING_LOW_HIGH Zero terminated string data. Two characters are stored in each register,
      the first in the low byte and the second in the high byte.

  **Note:** For big-endian formats the _BE format is order in which an IEEE value would
  be stored on a big-endian machine, and _BE_BS swaps the bytes in each 16-bit word
  relative to IEEE specification.
  However, for little-endian formats the _LE format is byte-swapped within each 16-bit word 
  compared how the IEEE value would be be stored on a little-endian machine.  
  The _LE_BS format is the order in which an IEEE value would be stored on a little-endian machine.
  This is done for backwards compatibility, because that is how _LE has always been stored in
  previous versions of this modbus module.
- Converted the documentation from HTML to REst, and moved to
  https://epics-modbus.readthedocs.io/en/latest/.
- Converted the release notes from HTML to Github Markdown and moved to
  https://github.com/epics-modules/modbus/blob/master/RELEASE.md.
- Improved debugging by returning the actual number of bytes transfered in read operations in
  the interpose interface functions.
- Fixed writing strings with absolute addressing.
- Fixed the length limit of function codes 1,2,5 and 15. 
  It was previously allowing 2015 and 1983 bits, rather than 2000 and 1968.
- Added support for specifying the maximum length of strings in the drvUser field.
  STRING_HIGH=17 would result in a 16 character long string (plus terminating 0 byte)
- Handle errors in drvModbusAsynConfigure.

Thanks to Krisztian Loki from ESS for the ZSTRING support and the last 4 fixes above.

## R3-0 (August 9, 2019)
-   This is a major rewrite of the driver, changing from C to C++
    inheriting from asynPortDriver. The major reason for the change is
    to be able to invoke the class methods such as
    drvModbusAsyn::doModbusIO() from other drivers. This allows drivers
    that communicate with Modbus devices to call methods to perform the
    Modbus I/O directly, which was not possible with the previous C API.
-   The new driver should be completely backwards compatible with
    previous versions, so no changes to IOC startup scripts or OPI
    displays are required.
-   A new example program, testClient.cpp, has been added. This program
    demonstrates how to instantiate a drvModbusAsyn object and use it to
    perform Modbus I/O to an external device. This example is a pure C++
    application running without an IOC. The same code could be used in a
    driver in an IOC.
-   Added OPI autoconvert files for edm, CSS BOY, and caQtDM.

## R2-11  (June 28, 2018)
-   Fixed problem with asynPrintIO. It was printing too few bytes under
    some conditions.
-   Added a counter of the number of I/O errors since the last
    successful I/O to the device. This counter is printed when the I/O
    returns to normal, and the counter is reset. Thanks to Scott Stubbs
    and Bruce Hill for this.

## R2-10-1 (September 15, 2017)
-   Previously the Modbus/TCP transaction ID was set to 1 on writes and
    ignored on reads. Now the Modbus transaction ID is incremented by 1
    on each message sent by the driver. On the reply the driver waits
    until a message with the correct transaction ID is received,
    ignoring messages with any other transaction ID. This is more
    robust, especially for Modbus/UDP links. Thanks to Eric Norum for
    this.

## R2-10 (September 15, 2017)
-   Added an epicsAtExit handler which stops the poller task. This
    eliminates error messages from the poller task when the IOC exits.
-   Greatly reduced the number of error messages printed when there are
    communication errors or timeouts with the Modbus device. Previously
    error messages were printed for each asyn port on each poller loop.
    Now, there is a single message per port when an error condition is
    detected, and a single message when the error condition ends. For
    example this is the output from a Koyo PLC with 3 drvModbusAsyn
    input ports that are polling at 10 Hz. The drvAsynIPPort has the
    asynOption `disconnectOnReadTimeout` set to `Y` set so that it
    reconnects quickly when the network is reconnected. The Ethernet
    cable was unplugged for about 8 seconds and then plugged back in.

        2017/09/12 13:14:25.764 drvModbusAsyn::doModbusIO port K1_Xn_Bit error calling writeRead, error=164.54.160.197:502 read error: S_errno_EWOULDBLOCK, nwrite=6/6, nread=0
        2017/09/12 13:14:25.781 drvModbusAsyn::doModbusIO port K1_Yn_In_Bit error calling writeRead, error=asynManager::queueLockPort queueRequest timed out, nwrite=0/6, nread=0
        2017/09/12 13:14:25.814 drvModbusAsyn::doModbusIO port K1_Cn_In_Bit error calling writeRead, error=asynManager::queueLockPort queueRequest timed out, nwrite=0/6, nread=0
        2017/09/12 13:14:33.564 drvModbusAsyn::doModbusIO port K1_Xn_Bit writeRead status back to normal nwrite=6/6, nread=6
        2017/09/12 13:14:33.581 drvModbusAsyn::doModbusIO port K1_Yn_In_Bit writeRead status back to normal nwrite=6/6, nread=6
        2017/09/12 13:14:33.614 drvModbusAsyn::doModbusIO port K1_Cn_In_Bit writeRead status back to normal nwrite=6/6, nread=34

    The first error message indicates that the K1\_Xn\_Bit driver got a
    timeout when reading the device. That timeout causes drvAsynIPPort
    driver to disconnect the port because of the asynOption described
    above. The next 2 errors are from the other two ports which get
    queueRequest timeouts because the port is now disconnected. When the
    network cable is reconnected each of the ports prints a `status
    back to normal` message.

-   The example IOC startup scripts now call
    asynSetOption(`disconnectOnReadTimeout`, `Y`). for TCP ports.
    This ensures much faster reconnection when the Modbus device comes
    back online if it disconnects. Note that for this to work asyn R4-32
    or later is needed, because of a needed fix to
    asynManager::queueLockPort(), which is used by the asynXXXSyncIO
    functions.

## R2-9 (August 23, 2016)
-   Added support for specifying absolute Modbus addresses in the asyn
    `addr` field. Previously each driver was limited to addressing at
    most 125 registers (read operations) or 123 registers (write
    operations), and the asyn `addr` field specified an offset
    relative to the modbusStartAddress passed as the fifth argument to
    drvAsynModbusConfigure(). Now if the modbusStartAddress=-1 then the
    driver will use absolute addressing, and the asyn `addr` specifies
    the absolute Modbus address in the range 0 to 65535. In this case
    the modbusLength argument to drvAsynModbusConfigure() is the maximum
    length required for any single Modbus transaction by that driver.
    This would be 1 if all Modbus reads and writes are for 16-bit
    registers, but it would be 4 if 64-bit floats (4 16-bit registers)
    are being used, and 100 (for example) if an Int32 waveform record
    with NELM=100 and a 16-bit Modbus data type is being read or
    written.
-   Added support for strings using the asynOctet interface. Some
    vendors use Modbus for string data. The Modbus standard does not
    specify how strings should be stored, and different vendors do it
    differently. The driver supports 4 new Modbus data types, using the
    following drvInfo strings:
    -   STRING\_HIGH One character is stored in the high byte of each
        register.
    -   STRING\_LOW One character is stored in the low byte of each
        register.
    -   STRING\_HIGH\_LOW Two characters are stored in each register,
        the first in the high byte and the second in the low byte.
    -   STRING\_LOW\_HIGH Two characters are stored in each register,
        the first in the low byte and the second in the high byte.

    The standard asynOctet device support is used. This supports
    stringout, stringin, and waveform records. Waveform records can be
    used for either input or output. The stringin and stringout records
    are limited to 40 characters strings, while waveform records are
    limited only by the 125/123 register limit of Modbus for read/write
    operatations i.e. up to 250 characters.
-   Added error checking to make sure the length of the reply from the
    Modbus server is the expected value. Previously garbled
    communications could cause a crash because of array bounds
    violation.

## R2-8 (February 11, 2016)
-   Improved the logic in the poller thread.
    -   Previously there was a call to epicsThreadSleep at the end of
        the poller loop. This meant that the poller always ran at least
        once, which might not be desirable. It also meant that one could
        only temporarily disable the poller by calling
        `pasynManager-\>lockPort()`, or to make it run infrequently by
        setting POLL\_DELAY to a long sleep time. In R2-5 the
        MODBUS\_READ command was added. However, this just did a read
        operation without triggering the poller thread, so records with
        I/O Intr scan would not process. It also required disabling the
        poller thread with `pasynManager-\>LockPort`.
    -   The poller thread has been changed so that it calls
        epicsEventWait or epicsEventWaitWithTimeout at the beginning of
        the poller loop. If the POLL\_DELAY is \>0 then it calls
        epicsEventWaitWithTimeout, using the POLL\_DELAY as the timeout.
        If POLL\_DELAY is ≤0 then it calls epicsEventWait, with no
        timeout. This allows the poller to be suspended indefinitely and
        only run when the epicsEvent is signaled. It also means if
        POLL\_DELAY ≤0 then the poller does not run at all until the
        epicsEvent is signaled. The MODBUS\_READ command now signals the
        epicsEvent to trigger the poller, rather than directly doing a
        read operation.
    -   Added a new template file, poll\_trigger.template which loads a
        record with the MODBUS\_READ drvInfo string to trigger the
        poller.
-   Fixed problems when using waveform records with the asynInt32Array
    interface.
    -   If the Modbus data type was not UINT16 or INT16 then the data
        were not read into the waveform record correctly. This bug has
        been fixed, and new files iocBoot/iocTest/array\_test.cmd and
        array\_test.substitions were added to test it using the Modbus
        Slave Simulator. New adl files, modbusTestArray.adl and
        array\_test.adl were added for these tests. modbusTestArray.adl
        also contains controls for POLL\_DELAY and POLL\_TRIGGER to
        verify the improvements to the poller thread above.
    -   Previously the first element in the waveform record was always
        the first register in that modbus driver, i.e. it did not allow
        specifying a register offset using the asyn ADDR address. This
        was improved so now one can specify an offset using the OFFSET
        macro parameter, as with the non-array records in modbus.


## R2-7-1 (May 6, 2015)
-   Minor changes to allow building dynamically on Cygwin.


## R2-7 (August 19, 2014)
-   Added support for function code 23, Read/Write Multiple Registers.
    Because of the architecture of the EPICS Modbus support, a driver
    that uses function 23 is restricted to being either a read-only
    driver where no data is written, or a write-only driver where no
    data is read. It is preferable to use functions codes 3, 4, 6, or 16
    if they are supported. However, some older devices only support
    function code 23.


## R2-6 (April 10, 2014)
-   Fixed a problem with function code 6 (write single register) on Wago
    devices when the MASK was not 0x0 or 0xFFFF. In this case a
    read/modify/write operation is required, and the driver was reading
    from the wrong address. On Wago devices the readback address for a
    register is offset by 0x200 from the write address. This offset was
    not being applied.
-   Added 2 new records to statistics.template. \$(P)\$(R)HistTimeAxis
    is a waveform record that contains the X time axis for histogram
    plots, and MsPerBin is the time per histogram bin in msec.


## R2-5 (October 2, 2013)
-   Fixed a mutex issue with the input poller. Previous versions had a
    potential problem with input records which did not use Scan=I/O
    Intr. In this case there was no mutex protecting access to the read
    buffer between the device support thread and the poller thread that
    was reading the Modbus data. It is likely that this problem was
    rarely if ever encountered because input records were almost always
    I/O Intr scanned. The fix was to remove the existing mutex in the
    driver and to simply call `pasynManager-\>lockPort()` for the entire
    duration of the polling cycle. This fix has a positive side effect:
    it is now possible for external clients, for example other drivers
    that call the Modbus driver, to temporarily disable the poller by
    calling `pasynManager-\>lockPort()`. This can be used to allow atomic
    sequences of Modbus read/write operations with no possibility of
    interference from the poller.
-   The above fix means that Modbus input drivers can now block on read
    operations, because they may need to wait for the poller to
    complete. These drivers are thus now created with the ASYN\_CANBLOCK
    flag, and the documentation has been changed to state that input
    drivers are now asynchronous, not synchronous.
-   Changed the wait for interruptAccept in the poller thread. It now
    works to have a very long polling time, but still having one initial
    read cycle and callback on I/O Intr scanned records.
-   Added a new command to the Modbus driver, which has the drvUser
    string `MODBUS\_READ`. This command is implemented on the
    asynInt32 interface. Calling `asynInt32-\>write()` with this command
    causes a Modbus I/O cycle for this driver. It will typically be used
    to force a Modbus input driver to do a read operation independent of
    the poller thread.
-   Added a new test driver called testModbusSyncIO.cpp. This driver is
    derived from asynPortDriver. It is designed to demonstrate using
    another asyn driver to communicate through the Modbus driver. This
    concept is currently being used to have a motor driver that
    communicates through the Modbus driver. It demonstrates two types of
    communication:
    1.  Using pasynInt32SyncIO calls to make `synchronous` calls to
        the Modbus driver. These calls are blocking and are queued.
    2.  Doing an atomic `read/modify/write` cycle using the following
        sequence of operations. This uses `pasynManager-\>lockPort` to
        disable the poller and allow this thread to directly call the
        `asynInt32-\>write()` and `asynInt32-\>read()` functions. It uses
        the following logic:
        -   Lock the output port. Needed because we are directly calling
            the write() function.
        -   Lock the input port, which disables the poller and because
            we are directly calling read().
        -   Sleep for 1 second so we can prove the poller was idle.
        -   Force a read of the Modbus input by writing on asynInt32
            interface with the MODBUS\_READ drvInfo.
        -   Read the input value on the asynInt32 interface.
        -   Add the value passed to this function to the current value
            just read.
        -   Sleep for 2 seconds so we can prove the poller was idle.
        -   Write the new value to the output driver.
        -   Unlock the input port.
        -   Unlock the output port.

        This new example driver can be tested with the new files
        iocBoot/iocTest/sim3.cmd and sim3.substitutions.
-   Minor change to structure packing declarations to allow building on
    ARM architecture with GCC 4.2.1


## R2-4 (May 22, 2012)
-   Improved support for different Modbus data types.
    -   Added support for 7 new Modbus data types:
        -   16-bit signed integer
        -   32-bit integer, little-endian
        -   32-bit integer, big-endian
        -   32-bit float, little-endian
        -   32-bit float, big-endian
        -   64-bit float, little-endian
        -   64-bit float, big-endian
    -   Added support to allow the Modbus data type to be specified on a
        per-record bas using the asyn drvUser field. Previously all
        records connected to a Modbus asyn port driver had the same
        Modbus data type, because each Modbus port driver was limited to
        a single data type.
    -   If the drvUser string in the link specification is omitted, or
        if it is the default of MODBUS\_DATA then the record will use
        the modbusDataType defined in drvModbusAsynConfigure. This is
        backwards compatible, and existing IOCs will continue to work
        with no changes to databases or startup scripts.
    -   Added new example IOC script, substitutions file, and medm
        screens to test all data types. The testing was done with a
        Modbus slave simulator.
-   Added special treatment for Wago devices. These device are different
    from other Modbus devices because the address to read back a
    register is not the same as the address to write the register. The
    readback address is the write address plus 0x200. This means that in
    previous versions of this driver the initial readback value for
    Modbus write operations to Wago devices was incorrect. This was
    fixed by adding the 0x200 offset to the readback address if the
    plcType argument to drvModbusAsynConfigure contains the substring
    `Wago` (case sensitive).
-   Added support for passing status information back to device support
    in callbacks for I/O Intr scanned records. Support for this was
    added in asyn R4-19. This means that if the Modbus device
    communications returns errors that I/O Intr scanned records will now
    have their alarm status set correctly.


## R2-3 (Sept 9, 2011)
-   build changes
-   added .opi files for CSS-BOY


## R2-2 (March 13, 2011)
-   R2-0 introduced a 20 ms delay before each Modbus write. This was not
    correct, delays should only be needed for serial RTU. The Modicon
    Modbus Protocol Reference Guide says this must be at least 3.5
    character times, e.g. about 3.5ms at 9600 baud, for Serial RTU. An
    additional writeDelayMsec parameter was added to
    modbusInterposeConfig. It is the final parameter, so if it is not
    specified the default value is zero, which is appropriate when not
    using Serial RTU. In the startup script lines like the following:

            modbusInterposeConfig("Koyo1",0,5000)
            
    should be changed to:

            modbusInterposeConfig("Koyo1",0,5000,0)
            
    for no delay, or to:

            modbusInterposeConfig("Koyo1",0,5000,20)
            
    for a 20 ms delay.


## R2-1 (November 7, 2010)
-   Bug fix. Non-automatic connection to the Modbus server uses
    `pasynCommonSyncIO-\>connectDevice()`. The pasynUser being used for
    that operation was being created with `pasynOctetSyncIO-\>connect()`.
    That was always an error, it must be created with
    `pasynCommonSyncIO-\>connect()`. This error became serious with asyn
    R4-14, and non-automatic connection no longer worked.
-   Previous releases of modbus recommended setting NoAutoConnect=1 when
    configuring the TCP or serial port. That was probably because of
    problems in connection management in earlier versions of asyn. With
    asyn R4-14 this is no longer necessary, and NoAutoConnect=0, the
    normal default, can be used with no problems. The example scripts
    Koyo1.cmd and Koyo2.cmd in the iocBoot directory have been changed
    to enable automatic connection to the IP or serial driver.


## R2-0 (November 26, 2009)
-   Moved the slave address handling from the asynInterpose layer to the
    Modbus driver layer. This was done because handing it in the the
    interpose layer only allowed 1 slave address per asyn serial port or
    IP port. This did not allow a single serial port to be used with
    multiple Modbus devices on an RS-485 bus, for example. **NOTE: This
    requires all startup command scripts to be changed, because the
    syntax of the modbusInterposeConfig and drvModbusAsynConfigure
    commands has changed.** Thanks to Yves Lussignol from CEA in France
    for making these changes.


## R1-3 (September 19, 2008)
-   Changed modbusInterpose.c to replace `pasynOctet-\>writeRaw()` and
    `pasynOctet-\>readRaw()` with `pasynOctet-\>write()` and
    `pasynOctet-\>read()`, because the raw routines have been removed in
    asyn R4-10.
-   Changed the driver to use the asynStandardInterfaces interfaces
    added to asyn in R4-10.


## R1-2 (September 6, 2007)
-   Fixed bug in computing byteCount in WRITE\_MULTIPLE\_COILS function
    code.

There is a known limitation with using serial interfaces. It is not
currently possible to have multiple Modbus servers connected to a single
serial port. This is a limitation of the asynInterposeInterface
architecture used. It is fixed in release 2-0.


## R1-1 (April 30, 2007)
- Initial release of modbus module.
