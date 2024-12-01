Overview of Modbus
------------------

MODBUS is an application layer messaging protocol, positioned at level 7
of the OSI model, that provides client/server communication between
devices connected on different types of buses or networks. It is
typically used for communication with I/O systems, including
Programmable Logic Controllers (PLCs).

Modbus communication links
~~~~~~~~~~~~~~~~~~~~~~~~~~

Modbus supports the following 3 communication-link layers:

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
   :header-rows: 1
   :widths: auto

   * - Link type
     - Description
   * - TCP
     - TCP/IP using standard port 502.
   * - UDP
     - UDP/IP using standard port 502. The use of UDP/IP is not part of the MODBUS
       standard but is useful for FPGAs with Ethernet in firmware which may provide
       support only for UDP.  The only difference between TCP and UDP operation is
       that when using UDP a missing reply packet is not considered to
       be an error until the transaction has been attempted 5 times.
   * - RTU
     - RTU is normally run over serial communication links, i.e. RS-232,
       RS-422, or RS-485. RTU uses an additional CRC for packet checking. The
       protocol directly transmits each byte as 8 data bits, so uses "binary"
       rather than ASCII encoding. When using serial links start and end of
       message frames is detected by timing rather than by specific characters.
       RTU can also be run over TCP, though this is less common than the
       standard Modbus TCP without RTU.
   * - Serial ASCII
     - Serial protocol, which is normally run over serial communication links,
       i.e. RS-232, RS-422, or RS-485. Serial ASCII uses an additional LRC for
       packet checking. The protocol encodes each byte as 2 ASCII characters.
       The start and end of message frames is detected by specific characters
       (":" to start a message and CR/LF to end a message). This protocol is
       less efficient than RTU, but may be more reliable in some environments.
       ASCII can also be run over TCP, though this is much less common than the
       standard Modbus TCP.

This **modbus** package supports all of the above Modbus
communication-link layers.

Modbus data types
~~~~~~~~~~~~~~~~~

Modbus provides access to the following 4 types of data:

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
   :header-rows: 1
   :widths: auto

   * - Primary tables
     - Object type
     - Access
     - Comments
   * - Discrete Inputs
     - Single bit
     - Read-Only
     - This type of data can be provided by an I/O system.
   * - Coils
     - Single bit
     - Read-Write
     - This type of data can be alterable by an application program.
   * - Input Registers
     - 16-bit word
     - Read-Only
     - This type of data can be provided by an I/O system.
   * - Holding Registers
     - 16-bit word
     - Read-Write
     - This type of data can be alterable by an application program.

Modbus communications
~~~~~~~~~~~~~~~~~~~~~

Modbus communication consists of a *request message* sent from a
*Modbus master* (client) to a *Modbus slave* (server). The server replies with a
*response message*. Modbus request messages contain:

-  An 8-bit Modbus function code that describes the type of data
   transfer to be performed.
-  A 16-bit Modbus address that describes the location in the server to
   read or write data from.
-  For write operations, the data to be transferred.

Modbus function codes
~~~~~~~~~~~~~~~~~~~~~

**modbus** supports the following 9 Modbus function codes:

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Access
    - Function description
    - Function code
  * - Bit access
    - Read Coils
    - 1
  * - Bit access
    - Read Discrete Inputs
    - 2
  * - Bit access
    - Write Single Coil
    - 5
  * - Bit access
    - Write Multiple Coils
    - 15
  * - 16-bit word access
    - Read Input Registers
    - 4
  * - 16-bit word access
    - Read Holding Registers
    - 3
  * - 16-bit word access
    - Write Single Register
    - 6
  * - 16-bit word access
    - Write Multiple Registers
    - 16
  * - Byte access (vendor defined)
    - Report Slave ID
    - 17
  * - 16-bit word access
    - Read/Write Multiple Registers
    - 23


Modbus addresses
~~~~~~~~~~~~~~~~

Modbus addresses are specified by a 16-bit integer address. The location
of inputs and outputs within the 16-bit address space is not defined by
the Modbus protocol, it is vendor-specific. The following table lists
some of the commonly used Modbus addresses for Koyo DL05/06/240/250/260/430/440/450 PLCs.

Discrete inputs and coils
_________________________

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - PLC Memory Type
    - Modbus start address Decimal (octal)
    - Function codes
  * - Inputs (X)
    - 2048 (04000)
    - 2
  * - Special Relays (SP)
    - 3072 (06000)
    - 2
  * - Outputs (Y)
    - 2048 (04000)
    - 1, 5, 15
  * - Control Relays (C)
    - 3072 (06000)
    - 1, 5, 15
  * - Timer Contacts (T)
    - 6144 (014000)
    - 1, 5, 15
  * - Counter Contacts (CT)
    - 6400 (014400)
    - 1, 5, 15
  * - Stage Status Bits (S)
    - 6144 (012000)
    - 1, 5, 15

Input registers and holding registers (V memory)
________________________________________________

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - PLC Memory Type
    - Modbus start address Decimal (octal)
    - Function codes
  * - Timer Current Values (TA)
    - 0 (00)
    - 4
  * - Counter Current Values (CTA)
    - 512 (01000)
    - 4
  * - Global Inputs (VGX)
    - 16384 (040000)
    - 4
  * - Global Outputs (VGY)
    - 16512 (040200)
    - 3, 6, 16
  * - Inputs (VX)
    - 16640 (040400)
    - 4
  * - Outputs (VY)
    - 16704 (040500)
    - 3, 6, 16
  * - Control Relays (VC)
    - 16768 (040600)
    - 3, 6, 16
  * - Stage Status Bits (VS)
    - 16896 (041000)
    - 3, 6, 16
  * - Timer Contacts (VT)
    - 16960 (041100)
    - 3, 6, 16
  * - Counter Contacts (VCT)
    - 16992 (041140)
    - 3, 6, 16
  * - Special Relays (VSP)
    - 17024 (041200)
    - 4

Other PLC manufacturers will use different Modbus addresses.

Note that 16-bit Modbus addresses are commonly specified with an offset
of 400001 (or 300001). This offset is not used by the **modbus** driver,
it uses only the 16-bit address, not the offset.

Modbus data length limitations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Modbus read operations are limited to transferring 125 16-bit words or
2000 bits. Modbus write operations are limited to transferring 123
16-bit words or 1968 bits.

Modbus exceptions
~~~~~~~~~~~~~~~~~

If a Modbus request is determined to be invalid by the server, it returns
a Modbus exception message.
The Modbus driver will print an error message if an exception is returned.
The following table describes the Modbus exception codes.

Modbus exception codes
______________________

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
  :header-rows: 1
  :widths: auto

  * - Exception code
    - Name
    - Meaning
  * - 0x01
    - Illegal Function
    - The function code received in the query is not an allowable action for the slave.
      This may be because the function code is only applicable to newer devices,
      and was not implemented in the unit selected.
      It could also indicate that the slave is in the wrong state to process a request of this type,
      for example because it is unconfigured and is being asked to return register values.
  * - 0x02
    - Illegal Data Address
    - The data address received in the query is not an allowable address for the slave.
      More specifically, the combination of reference number and transfer length is invalid.
      For a controller with 100 registers, a request with offset 96 and length 4 would succeed,
      a request with offset 96 and length 5 will generate exception 02.
  * - 0x03
    - Illegal Data Value
    - A value contained in the query data field is not an allowable value for the slave.
      This indicates a fault in the structure of remainder of a complex request,
      such as that the implied length is incorrect.
      It specifically does NOT mean that a data item submitted for storage in a register has a value
      outside the expectation of the application program, since the MODBUS protocol is unaware of the
      significance of any particular value of any particular register.
  * - 0x04
    - Slave Device Failure
    - An unrecoverable error occurred while the slave was attempting to perform the requested action.
  * - 0x05
    - Acknowledge
    - Specialized use in conjunction with programming commands.
      The slave has accepted the request and is processing it, but a long duration of time will be required to do so.
      This response is returned to prevent a timeout error from occurring in the master.
      The master can next issue a Poll Program Complete message to determine if processing is completed.
      NOTE: The EPICS Modbus driver does not print an error message for this response, since it is not really an error.
  * - 0x06
    - Slave Device Busy
    - Specialized use in conjunction with programming commands.
      The slave is engaged in processing a long-duration program command.
      The master should retransmit the message later when the slave is free.
  * - 0x07
    - Negative Acknowledge
    - The slave cannot perform the program function received in the query.
      This code is returned for an unsuccessful programming request using function code 13 or 14 decimal.
      The master should request diagnostic or error information from the slave.
  * - 0x08
    - Memory Parity Error
    - Specialized use in conjunction with function codes 20 and 21 and reference type 6,
      to indicate that the extended file area failed to pass a consistency check.
      The slave attempted to read extended memory or record file, but detected a parity error in memory.
      The master can retry the request, but service may be required on the slave device.
  * - 0x0A
    - Gateway Path Unavailable
    - Specialized use in conjunction with gateways, indicates that the gateway was unable to allocate an
      internal communication path from the input port to the output port for processing the request.
      Usually means the gateway is misconfigured or overloaded.
  * - 0x0B
    - Gateway Target Device Failed to Respond
    - Specialized use in conjunction with gateways, indicates that no response was obtained from the target device.
      Usually means that the device is not present on the network.

More information on Modbus
~~~~~~~~~~~~~~~~~~~~~~~~~~

For more information about the Modbus protocol, the official Modbus
specification can be found `on the
Web <http://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf>`__
or in the **modbus** documentation directory.
:download:`Modbus_Application_Protocol_V1_1b.pdf`.

The official specification for Modbus over TCP/IP can be found `on the
Web <http://www.modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1_0b.pdf>`__
or in the **modbus** documentation directory.
:download:`Modbus_Messaging_Implementation_Guide_V1_0b.pdf`.

The official specification for Modbus over serial can be found `on the
Web <http://www.modbus.org/docs/Modbus_over_serial_line_V1_02.pdf>`__ or
in the **modbus** documentation directory.
:download:`Modbus_over_serial_line_V1_02.pdf`.

