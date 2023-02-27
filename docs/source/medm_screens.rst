medm screens
------------

**modbus** provides example medm .adl files in the modbusApp/op/adl
directory. 

modbusDataTypes.adl
~~~~~~~~~~~~~~~~~~~
The following is a screen shot from an IOC running the testDataTypes.cmd
and testDataTypes.substitutions files, communicating with a Modbus Slave Simulator.
These are the ao/ai records using the asynFloat64 interface.
It shows that the output and input (readback) records agree.

.. figure:: testDataTypes.png
    :align: center

The following is a screen shot from the Modbus Slave Simulator communicating
with the ao/ai records shown above.  The values shown in this screen agree
with this in the medm screen, showing that each Modbus data type is being communicated correctly.

.. figure:: testDataTypesSimulator.png
    :align: center

The following are screen shots of these screens from an IOC
controlling a Koyo DL205 PLC.

Koyo1.adl
~~~~~~~~~
Top level medm screen for the Koyo1 example application.

.. figure:: Koyo1.png
    :align: center

Koyo_8inputs.adl
~~~~~~~~~~~~~~~~
Inputs X0-X7 read as discrete inputs (function code 1).

.. figure:: K1_Xn_Bit.png
    :align: center

Inputs C200-C207 read as register inputs (function code 6).

.. figure:: K1_C20n_In_Word.png
    :align: center

Koyo_8outputs.adl
~~~~~~~~~~~~~~~~~
Outputs Y0-Y7 written using register access (function code 6).

.. figure:: K1_Yn_Out_Word.png
    :align: center

Outputs Outputs C200-C207 written using bit access (function code 5).

.. figure:: K1_C20n_Out_Bit.png
    :align: center

modbusArray.adl
~~~~~~~~~~~~~~~
Inputs C0-C377 read using a waveform record and coil access (function code 1).

.. figure:: K1_Cn_In_Bit_Array.png
    :align: center

Inputs C0-C377 read using a waveform record and register access (function code 3).

.. figure:: K1_Cn_In_Word_Array.png
    :align: center

modbusStatistics.adl
~~~~~~~~~~~~~~~~~~~~
I/O statistics for the Modbus driver that is reading inputs X0-X37 using register access (function code 3). 
The histogram is the number of events versus TCP/IP write/read cycle time in msec.

.. figure:: K1_Xn_Bit_Statistics.png
    :align: center

Koyo2.adl
~~~~~~~~~
Top level medm screen for the Koyo2 example application.

.. figure:: Koyo2.png
    :align: center

Koyo_4ADC.adl
~~~~~~~~~~~~~
4 ADC inputs from a 13-bit bipolar ADC.

.. figure:: K2_ADCs.png
    :align: center

