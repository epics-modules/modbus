Driver architecture
-------------------

**CAUTION:** **modbus** can provide access to all of the I/O and memory
of the PLC. In fact, it is not even necessary to run a ladder logic
program in the PLC at all. The PLC can be used as a "dumb" I/O
subsystem, with all of the logic residing in the EPICS IOC. However, if
a ladder logic program *is* being run in the PLC then the EPICS access
with **modbus** must be designed carefully. For example, the EPICS IOC
might be allowed to *read* any of the PLC I/O points (X inputs, Y
outputs, etc.), but *writes* could be restricted to a small range of
Control Registers, (e.g. C200-C240). The ladder logic would monitor
these control registers, considering them to be "requests" from EPICS
that should be acted upon only if it is safe to do so.

The architecture of the **modbus** module from the top-level down
consists of the following 4 layers:

1. `EPICS asyn device support <https://epics-modules.github.io/master/asyn/R4-40/asynDriver.html#genericEpicsSupport>`__.
   This is the general purpose device support provided with
   `asyn <https://github.com/epics-modules/asyn>`__ There is no
   special device support needed or provided with **modbus**.
2. An EPICS asyn port driver that functions as a Modbus client. The
   **modbus** port driver communicates with EPICS device support (layer
   1) using the standard asyn interfaces (asynUInt32Digital, asynInt32,
   etc.). This driver sends and receives device-independent Modbus
   frames via the standard asynOctet interface to the "interpose
   interface" (layer 3). These frames are independent of the underlying
   communications protocol. Prior to R3-0 this driver was written in C.
   In R3-0 it was written as a C++ class that inherits from
   asynPortDriver. This allows it to export its methods in a way that is
   easy for other drivers to use, in particular the doModbusIO() method.
3. An asyn "interpose interface" layer that handles the additional data
   required by the underlying communications layer (TCP, RTU, ASCII).
   This layer communicates via the standard asynOctet interface to both
   the overlying Modbus driver (layer 2) and to the underlying asyn
   hardware port driver (layer 4).
4. An asyn port driver that handles the low-level communication (TCP/IP
   or serial). This is one of the standard port drivers provided with
   asyn, i.e.
   `drvAsynIPPort <https://epics-modules.github.io/master/asyn/R4-40/asynDriver.html#drvAsynIPPort>`__
   or
   `drvAsynSerialPort <https://epics-modules.github.io/master/asyn/R4-40/asynDriver.html#drvAsynSerialPort>`__.
   They are not part of the **modbus** module.

Because **modbus** makes extensive use of existing asyn facilities, and
only needs to implement layers 2 and 3 above, the amount of code in
**modbus** is quite small (fewer than 3,900 lines).

Each **modbus** port driver is assigned a single Modbus function code.
Usually a drivers is also assigned a single contiguous range of Modbus
memory, up to 2000 bits or 125 words. One typically creates several
**modbus** port drivers for a single PLC, each driver reading or writing
a different set of discrete inputs, coils, input registers or holding
registers. For example, one might create one port driver to read
discrete inputs X0-X37, a second to read control registers C0-C377, and
a third to write control registers C300-C377. In this case the asyn
address that is used by each record is relative to the starting address
for that driver.

It is also possible to create a driver is allowed to address any
location in the 16-bit Modbus address space. Each read or write
operation is still limited to the 125/123 word limits. In this case the
asyn address that is used by each record is the absolute Modbus address.
This absolute addressing mode is enabled by passing -1 as the
modbusStartAddress when creating the driver.

The restriction the modbus port driver to a single Modbus function does
not apply to the doModbusIO() method. This method can be used for
arbitrary Modbus IO using any function code. If absolute addressing is
enabled as described above then the doModbusIO() function can also
address any Modbus memory location.

The behavior of the port driver differs for read function codes (1, 2,
3, 4), write function codes (5, 6, 15, 16, 17), and read/write function
codes (23).

Modbus read functions
~~~~~~~~~~~~~~~~~~~~~

For read function codes (when absolute addressing is not being used) the
driver spawns a poller thread. The poller thread reads the entire block
of Modbus memory assigned to this port in a single Modbus transaction.
The values are stored in a buffer in the driver. The delay between polls
is set when the port driver is created, and can be changed later at
run-time. The values are read by EPICS using the standard asyn
interfaces (asynUInt32Digital, asynInt32, asynInt64, asynFloat64, etc.) The values
that are read are the last stored values from the poller thread. The
means that EPICS read operations are *asynchronous*, i.e. they can
block. This is because although they do not directly result in Modbus
I/O, they do need to wait for a mutex that indicates that the poller
thread is done.

For read functions it is possible to set the EPICS records to "I/O Intr"
scanning. If this is done then the port driver will call back device
support whenever there is new data for that input. This improves
efficiency, because such records only process when needed, they do not
need to be periodically scanned.

The previous paragraphs describe the normal configuration for read
operations, where relative Modbus addressing is being used. If absolute
addressing is being used then the driver does not create a poller
thread, because it does not know what parts of the Modbus address space
should be polled. In this case read records cannot have SCAN=I/O Intr.
They must either be periodically scanned, or scanned by directly causing
the record to process, such as writing 1 to the .PROC field. Each time
the record processes it will result in a separate Modbus read operation.
NOTE: This is **much** less efficient than reading many registers at
once with relative Modbus addressing. For this reason absolute Modbus
addressing with read functions should normally be avoided.

Modbus write functions
~~~~~~~~~~~~~~~~~~~~~~

For write function codes the driver does not itself create a separate
thread. Rather the driver does the Modbus I/O immediately in response to
the write operations on the standard asyn interfaces. This means that
EPICS write operations are also *asynchronous*, i.e. they block because
Modbus I/O is required. When the **modbus** driver is created it tells
asynManager that it can block, and asynManager creates a separate thread
that executes the write operations.

Word write operations using the asynUInt32Digital interface (with a mask
parameter that is not 0x0 or 0xFFFF) are done using read/modify/write
operations. This allows multiple Modbus clients to write and read single
words in the same block of Modbus memory. However, it *does not*
guarantee correct operation if multiple Modbus clients (or the PLC
itself) can modify bits within a single word. This is because the Modbus
server cannot perform the read/modify/write I/O as an atomic operation
at the level of the Modbus client.

For write operations it is possible to specify that a single read
operation should be done when the port driver is created. This is
normally used so that EPICS obtains the current value of an output
device when the IOC is initialized.

Modbus RTU specifies a minimum delay of 3.5 character times between
writes to the device. The modbusInterposeConfig function allows one to
specify a write delay in msec before each write.

Modbus write/read functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Modbus function code 23 allows for writing a set of registers and
reading a set of registers in a single operation. The read operation is
performed after the write operation, and the register range to be read
can be different from the register range to be written. Function code 23
is not widely used, and the write/read operation is not a good fit to
the **modbus** driver model of read-only and write-only drivers.
Function code 23 is implemented in **modbus** with the following
restrictions:

-  A driver that uses Modbus function code 23 is either *read-only* or
   *write-only*.
-  A read-only driver is created by specifying function code 123 to the
   drvModbusAsynConfigure command described below. The driver will use
   Modbus function code 23 for the Modbus protocol. It will only read
   registers (like function codes 3 and 4), it will not write any data
   to the device.
-  A write-only driver is created by specifying function code 223 to the
   drvModbusAsynConfigure command described below. The driver will use
   Modbus function code 23 for the Modbus protocol. It will only write
   registers (like function code 16), it will not read any data from the
   device.

Platform independence
~~~~~~~~~~~~~~~~~~~~~

**modbus** should run on all EPICS platforms. It has been tested on
linux-x86, linux-x86_64, vxWorks-ppc32, win32-x86, windows-x64, (native
Windows with Microsoft Visual Studio C++ compiler).

The only thing that may be architecture dependent in **modbus** is the
structure packing in modbus.h. The "#pragma pack(1)" directive used
there is supported on gnu and Microsoft compilers. If this directive is
not supported on some compilers of interest then modbus.h will need to
have the appropriate architecture dependent code added.
