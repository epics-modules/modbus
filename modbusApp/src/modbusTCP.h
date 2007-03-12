/*
 * File:        modbusTCP.h
 *
 * Notes:  This file includes a number of defines and structures for generating
 * modbus messages over tcpip.  Note that the structures packed.
 *
 *=fdoc=====================================================================*/

#ifndef MODBUSTCP_H
#define MODBUSTCP_H

/* Modbus function codes */
#define MODBUS_READ_COILS               0x01
#define MODBUS_READ_DISCRETE_INPUTS     0x02
#define MODBUS_READ_HOLDING_REGISTERS   0x03
#define MODBUS_READ_INPUT_REGISTERS     0x04
#define MODBUS_WRITE_SINGLE_COIL        0x05
#define MODBUS_WRITE_SINGLE_REGISTER    0x06
#define MODBUS_WRITE_MULTIPLE_COILS     0x0F
#define MODBUS_WRITE_MULTIPLE_REGISTERS 0x10

#define MODBUS_EXCEPTION_FCN            0x80

#pragma pack(1)

/* All Modbus messages over TCP/IP are preceeded by a header */

typedef struct modbusMBAPHeader_str
{
    unsigned short transactId;
    unsigned short protocolType;
    unsigned short cmdLength;
    unsigned char  destId;
} modbusMBAPHeader;


/*---------------------------------------------*/
/* structure definitions for Modbus requests */
/*---------------------------------------------*/

typedef struct modbusReadRequest_str
{
    modbusMBAPHeader mbapHeader;
    unsigned char    fcode;
    unsigned short   startReg;
    unsigned short   numRead;
} modbusReadRequest;

typedef struct modbusReadResponse_str
{
    modbusMBAPHeader mbapHeader;
    unsigned char  fcode;
    unsigned char  byteCount;
    unsigned char  data[1];
} modbusReadResponse;

typedef struct modbusWriteSingleRequest_str
{
    modbusMBAPHeader mbapHeader;
    unsigned char  fcode;
    unsigned short regStart;
    unsigned short data;
} modbusWriteSingleRequest;


typedef struct modbusWriteSingleResponse_str
{
    modbusMBAPHeader mbapHeader;
    unsigned char  fcode;
    unsigned short regStart;
    unsigned short data;
} modbusWriteSingleResponse;

typedef struct modbusWriteMultipleRequest_str
{
    modbusMBAPHeader mbapHeader;
    unsigned char  fcode;
    unsigned short regStart;
    unsigned short numOutput;
    unsigned char  byteCount;
    unsigned char  data[1];
} modbusWriteMultipleRequest;


typedef struct modbusWriteMultipleResponse_str
{
    modbusMBAPHeader mbapHeader;
    unsigned char  fcode;
    unsigned short regStart;
    unsigned short numOutput;
} modbusWriteMultipleResponse;

typedef struct modbusExceptionResponse_str
{
    modbusMBAPHeader mbapHeader;
    unsigned char  fcode;
    unsigned char  exception;
} modbusExceptionResponse;

#endif 
