/*
 * File:        modbus.h
 *
 * Notes:  This file includes a number of defines and structures for generating
 * modbus messages.  Note that the structures are packed.
 *
 * Author: Mark Rivers
 */

#ifndef MODBUS_H
#define MODBUS_H

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

#define MAX_MODBUS_FRAME_SIZE 600       /* Buffer size for input and output packets.
                                         * 513 (max for ASCII serial) should be enough, 
                                         * but we are being safe. */


/* Pack all structures defined here on 1-byte boundaries */
#pragma pack(1)

/* All Modbus messages over TCP/IP are preceeded by the MBAP header */

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
    unsigned char    fcode;
    unsigned short   startReg;
    unsigned short   numRead;
} modbusReadRequest;

typedef struct modbusReadResponse_str
{
    unsigned char  fcode;
    unsigned char  byteCount;
    unsigned char  data[1];
} modbusReadResponse;

typedef struct modbusWriteSingleRequest_str
{
    unsigned char  fcode;
    unsigned short startReg;
    unsigned short data;
} modbusWriteSingleRequest;


typedef struct modbusWriteSingleResponse_str
{
    unsigned char  fcode;
    unsigned short startReg;
    unsigned short data;
} modbusWriteSingleResponse;

typedef struct modbusWriteMultipleRequest_str
{
    unsigned char  fcode;
    unsigned short startReg;
    unsigned short numOutput;
    unsigned char  byteCount;
    unsigned char  data[1];
} modbusWriteMultipleRequest;


typedef struct modbusWriteMultipleResponse_str
{
    unsigned char  fcode;
    unsigned short startReg;
    unsigned short numOutput;
} modbusWriteMultipleResponse;

typedef struct modbusExceptionResponse_str
{
    unsigned char  fcode;
    unsigned char  exception;
} modbusExceptionResponse;

/* Revert to packing that was in effect when compilation started */
#pragma pack()

#endif 
