/* modbusInterpose.c */

/* 
 * Modbus interpose interfaces for asyn.  The Modbus driver builds Modbus frames
 * and sends them to the drvAsynIPPort or drvAsynSerialPort drivers.  This
 * file implements the interpose interface that adds the required header information
 * for TCP, RTU serial or ASCII serial before sending the frame to the underlying
 * driver.  It removes header information from the response frame before returning it
 * to the Modbus driver.
 *
 * Author: Mark Rivers
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <cantProceed.h>
#include <epicsAssert.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <osiSock.h>
#include <iocsh.h>

#include <epicsThread.h>
#include "asynDriver.h"
#include "asynOctet.h"
#include "modbusInterpose.h"
#include "modbus.h"
#include <epicsExport.h>

static char *driver="modbusInterpose";

#define DEFAULT_TIMEOUT 2.0

/* Table of CRC values for high-order byte */
static unsigned char CRC_Lookup_Hi[] = {
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
0x40
};

/* Table of CRC values for low-order byte */
static unsigned char CRC_Lookup_Lo[] = {
0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
0x40
};

typedef struct modbusPvt {
    char           *portName;
    double         timeout;
    double         writeDelay;
    asynInterface  modbusInterface;
    asynOctet      *pasynOctet;           /* Table for low level driver */
    void           *octetPvt;
    modbusLinkType linkType;
    asynUser       *pasynUser;
    int            transactionId;
    char           buffer[MAX_MODBUS_FRAME_SIZE];
} modbusPvt;
    
/* asynOctet methods */
static asynStatus writeIt(void *ppvt,asynUser *pasynUser,
    const char *data,size_t numchars,size_t *nbytesTransfered);
static asynStatus readIt(void *ppvt,asynUser *pasynUser,
    char *data,size_t maxchars,size_t *nbytesTransfered,int *eomReason);
static asynStatus flushIt(void *ppvt,asynUser *pasynUser);
static asynStatus registerInterruptUser(void *ppvt,asynUser *pasynUser,
    interruptCallbackOctet callback, void *userPvt,void **registrarPvt);
static asynStatus cancelInterruptUser(void *drvPvt,asynUser *pasynUser,
     void *registrarPvt);
static asynStatus setInputEos(void *ppvt,asynUser *pasynUser,
    const char *eos,int eoslen);
static asynStatus getInputEos(void *ppvt,asynUser *pasynUser,
    char *eos,int eossize ,int *eoslen);
static asynStatus setOutputEos(void *ppvt,asynUser *pasynUser,
    const char *eos,int eoslen);
static asynStatus getOutputEos(void *ppvt,asynUser *pasynUser,
    char *eos,int eossize,int *eoslen);
static asynOctet octet = {
    writeIt,readIt,flushIt,
    registerInterruptUser, cancelInterruptUser,
    setInputEos,getInputEos,setOutputEos,getOutputEos
};


epicsShareFunc int modbusInterposeConfig(const char *portName,
                                         modbusLinkType linkType, 
                                         int timeoutMsec, int writeDelayMsec)
{
    modbusPvt     *pPvt;
    asynInterface *pasynInterface;
    asynStatus    status;
    asynUser      *pasynUser;

    pPvt = callocMustSucceed(1, sizeof(*pPvt), "modbusInterposeConfig");
    pPvt->portName = epicsStrDup(portName);
    pPvt->linkType = linkType;
    pPvt->timeout = timeoutMsec/1000.;
    pPvt->writeDelay = writeDelayMsec/1000.;
    if (pPvt->timeout == 0.0) pPvt->timeout = DEFAULT_TIMEOUT;
    pPvt->modbusInterface.interfaceType = asynOctetType;
    pPvt->modbusInterface.pinterface = &octet;
    pPvt->modbusInterface.drvPvt = pPvt;
    pasynUser = pasynManager->createAsynUser(0,0);
    pPvt->pasynUser = pasynUser;
    pPvt->pasynUser->userPvt = pPvt;
    status = pasynManager->connectDevice(pasynUser,portName,0);
    if(status!=asynSuccess) {
        printf("%s connectDevice failed\n",portName);
        goto bad;
    }
    /* Find the asynOctet interface */
    pasynInterface = pasynManager->findInterface(pasynUser, asynOctetType, 1);
    if (!pasynInterface) {
        printf("%s findInterface error for asynOctetType %s\n",
               portName, pasynUser->errorMessage);
        goto bad;
    }
    
    status = pasynManager->interposeInterface(portName, 0,
       &pPvt->modbusInterface, &pasynInterface);
    if(status!=asynSuccess) {
        printf("%s interposeInterface failed\n", portName);
        goto bad;
    }
    pPvt->pasynOctet = (asynOctet *)pasynInterface->pinterface;
    pPvt->octetPvt = pasynInterface->drvPvt;

    return(0);
    
    bad:
    pasynManager->freeAsynUser(pasynUser);
    free(pPvt);
    return -1;
}


static void computeCRC(char *buffer, int nchars, 
                       unsigned char *CRC_Lo, unsigned char *CRC_Hi) 
{
    int CRC_Index ;              /* will index into CRC lookup table */
    int i;

    /* This algorithm is taken from the official Modbus over serial line documentation */
    *CRC_Hi = 0xFF; /* high byte of CRC initialized */
    *CRC_Lo = 0xFF; /* low byte of CRC initialized */
    for (i=0; i<nchars; i++) {
        CRC_Index = *CRC_Lo ^ (unsigned char)buffer[i];
        *CRC_Lo = *CRC_Hi ^ CRC_Lookup_Hi[CRC_Index];
        *CRC_Hi = CRC_Lookup_Lo[CRC_Index];
    }
}

static void computeLRC(char *buffer, int nchars, unsigned char *LRC) 
{
    int i;

    *LRC = 0;
    for (i=0; i<nchars; i++) {
        *LRC += buffer[i];
    }
    *LRC = (unsigned char) -((char)*LRC);
}

static void encodeASCII(char *buffer, unsigned char value) 
{
    unsigned char temp;
    
    temp = value >> 4;
    if (temp < 10) temp+= '0'; else temp+= 'A'-10;
    *buffer++ = (char)temp;
    temp = value & 0x0F;
    if (temp < 10) temp+= '0'; else temp+= 'A'-10;
    *buffer = (char)temp;
}

static void decodeASCII(char *buffer, char *value) 
{
    char temp;
    unsigned char uvalue;
    
    temp = *buffer++;
    if (temp > '9') uvalue = temp - 'A' + 10; else uvalue = temp - '0';
    uvalue = uvalue << 4;
    temp = *buffer;
    if (temp > '9') uvalue += temp - 'A' + 10; else uvalue += temp - '0';
    *value = (char)uvalue;
}


/* asynOctet methods */
static asynStatus writeIt(void *ppvt, asynUser *pasynUser,
                          const char *data, size_t numchars,
                          size_t *nbytesTransfered)
{
    modbusPvt  *pPvt = (modbusPvt *)ppvt;
    asynStatus status = asynSuccess;
    size_t     nbytesActual = 0;
    size_t     nWrite;
    modbusMBAPHeader mbapHeader;
    unsigned short cmdLength = numchars;
    unsigned short modbusEncoding=0;
    int mbapSize = sizeof(modbusMBAPHeader);
    unsigned char CRC_Hi;
    unsigned char CRC_Lo;
    unsigned char LRC;
    char *pout;
    int i;

    if (pPvt->writeDelay > 0.0) epicsThreadSleep(pPvt->writeDelay);
    
    pasynUser->timeout = pPvt->timeout;

    switch(pPvt->linkType) {
        case modbusLinkTCP:
            /* Build the MBAP header */
            pPvt->transactionId = (pPvt->transactionId + 1) & 0xFFFF;
            mbapHeader.transactId    = htons(pPvt->transactionId);
            mbapHeader.protocolType  = htons(modbusEncoding);
            mbapHeader.cmdLength     = htons(cmdLength);
 
            /* Copy the MBAP header to the local buffer */
            memcpy(pPvt->buffer, &mbapHeader, mbapSize);

            /* Copy the Modbus data to the local buffer */
            memcpy(pPvt->buffer + mbapSize, data, numchars);

            /* Send the frame with the underlying driver */
            nWrite = numchars + mbapSize;
            status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser,
                                             pPvt->buffer, nWrite, 
                                             &nbytesActual);
            *nbytesTransfered = (nbytesActual > numchars) ? numchars : nbytesActual;
            break;

        case modbusLinkRTU:
            /* First byte in the output is the slave address */
            /* Next is the Modbus data */
            memcpy(pPvt->buffer, data, numchars);
            /* Compute the CRC */
            computeCRC(pPvt->buffer, numchars, &CRC_Lo, &CRC_Hi);
            pPvt->buffer[numchars] = CRC_Lo;
            pPvt->buffer[numchars+1] = CRC_Hi;
            /* Send the frame with the underlying driver */
            nWrite = numchars + 2;
            status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser,
                                             pPvt->buffer, nWrite, 
                                             &nbytesActual);
            *nbytesTransfered = (nbytesActual > numchars) ? numchars : nbytesActual;
            break;

        case modbusLinkASCII:
            /* Put slave address and data in buffer to compute LRC */
            memcpy(pPvt->buffer, data, numchars);
            computeLRC(pPvt->buffer, numchars, &LRC);
            /* Now convert to ASCII */
            /* First byte in the output is : */
            pout = pPvt->buffer;
            *pout = ':';
            pout++;
            /* Next is the slave address */
            for (i=0; i<numchars; i++) {
                encodeASCII(pout, data[i]);
                pout+=2;
            }
            /* Next is the LRC */
            encodeASCII(pout, LRC);
            pout+=2;
            /* The driver will add the CR/LF */
            /* Send the frame with the underlying driver */
            nWrite = pout - pPvt->buffer;
            status = pPvt->pasynOctet->write(pPvt->octetPvt, pasynUser,
                                             pPvt->buffer, nWrite, 
                                             &nbytesActual);
            *nbytesTransfered = (nbytesActual > numchars) ? numchars : nbytesActual;

            break;
    }
    return status;
}


static asynStatus readIt(void *ppvt, asynUser *pasynUser,
                         char *data, size_t maxchars, size_t *nbytesTransfered,
                         int *eomReason)
{
    modbusPvt *pPvt = (modbusPvt *)ppvt;
    size_t nRead;
    size_t nbytesActual;
    asynStatus status = asynSuccess;
    int mbapSize = sizeof(modbusMBAPHeader);
    unsigned char CRC_Hi;
    unsigned char CRC_Lo;
    unsigned char LRC;
    int i;
    char *pin;

    pasynUser->timeout = pPvt->timeout;

    /* Set number read to 0 in case of errors */
    *nbytesTransfered = 0;
    
    switch(pPvt->linkType) {
        case modbusLinkTCP:
            nRead = maxchars + mbapSize + 1;
            for (;;) {
                status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser,
                                                pPvt->buffer, nRead, 
                                                &nbytesActual, eomReason);
                if (status != asynSuccess) return status;
                if (nbytesActual >= 2) {
                    int id = ((pPvt->buffer[0] & 0xFF)<<8)|(pPvt->buffer[1]&0xFF);
                    if (id == pPvt->transactionId) break;
                }
            }
            /* Copy bytes beyond mbapHeader to output buffer */
            nRead = nbytesActual;
            nRead = nRead - mbapSize - 1;
            if (nRead < 0) nRead = 0;
            if (nRead > maxchars) nRead = maxchars;
            if (nRead > 0) memcpy(data, pPvt->buffer + mbapSize + 1, nRead);
            if(nRead<maxchars) data[nRead] = 0; /*null terminate string if room*/
            *nbytesTransfered = nRead;
            break;

        case modbusLinkRTU:
            nRead = maxchars + 3;
            status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser,
                                            pPvt->buffer, nRead,
                                            &nbytesActual, eomReason);
            if (status != asynSuccess) return status;
            /* Compute and check the CRC including the CRC bytes themselves, 
             * should be 0 */
            computeCRC(pPvt->buffer, nbytesActual, &CRC_Lo, &CRC_Hi);
            if ((CRC_Lo != 0) || (CRC_Hi != 0)) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR,
                          "%s::readIt, CRC error\n",
                          driver);
                return asynError;
            }
            /* Copy bytes beyond address to output buffer */
            nRead = nbytesActual;
            nRead = nRead - 3;
            if (nRead < 0) nRead = 0;
            if (nRead > maxchars) nRead = maxchars;
            if (nRead > 0) memcpy(data, pPvt->buffer + 1, nRead);
            if (nRead<maxchars) data[nRead] = 0; /*null terminate string if room*/
            *nbytesTransfered = nRead;
            break;

        case modbusLinkASCII:
            /* The maximum number of characters is 2*maxchars + 7 
             * (7= :(1), address(2), LRC(2), CR/LF(2) */
            nRead = maxchars*2 + 7;
            status = pPvt->pasynOctet->read(pPvt->octetPvt, pasynUser,
                                            pPvt->buffer, nRead,
                                            &nbytesActual, eomReason);
            if (status != asynSuccess) return status;
            pin = pPvt->buffer;
            if (*pin != ':') return asynError;
            pin += 1;
            for (i=0; i<(nbytesActual-1)/2; i++) {
                decodeASCII(pin, &data[i]);
                pin+=2;
            }
            /* Number of bytes in buffer for computing LRC is i */
            nRead = i;
            computeLRC(data, nRead, &LRC);
            if (LRC != data[i]) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR,
                          "%s::readIt, LRC error, nRead=%d, received LRC=0x%x, computed LRC=0x%x\n",
                          driver, (int)nRead, data[i], LRC);
                return asynError;
            }
            /* The buffer now contains binary data, but the first byte is address.  
             * and last byte is LRC; Copy over buffer so first byte is function code */
            nRead = nRead - 2;
            if (nRead < 0) nRead = 0;
            if (nRead > maxchars) nRead = maxchars;
            if (nRead > 0) memmove(data, data + 1, nRead);
            if (nRead<maxchars) data[nRead] = 0; /*null terminate string if room*/
            *nbytesTransfered = nRead;
            break;
    }
     
    return status;
}


static asynStatus flushIt(void *ppvt, asynUser *pasynUser)
{
    modbusPvt *pPvt = (modbusPvt *)ppvt;
    return pPvt->pasynOctet->flush(pPvt->octetPvt, pasynUser);
}

static asynStatus registerInterruptUser(void *ppvt, asynUser *pasynUser,
    interruptCallbackOctet callback, void *userPvt, void **registrarPvt)
{
    modbusPvt *pPvt = (modbusPvt *)ppvt;
    return pPvt->pasynOctet->registerInterruptUser(pPvt->octetPvt,
                                               pasynUser, callback, userPvt, 
                                               registrarPvt);
} 

static asynStatus cancelInterruptUser(void *drvPvt, asynUser *pasynUser,
     void *registrarPvt)
{
    modbusPvt *pPvt = (modbusPvt *)drvPvt;
    return pPvt->pasynOctet->cancelInterruptUser(pPvt->octetPvt,
                                             pasynUser,registrarPvt);
} 

static asynStatus setInputEos(void *ppvt, asynUser *pasynUser,
    const char *eos, int eoslen)
{
    modbusPvt *pPvt = (modbusPvt *)ppvt;
    return pPvt->pasynOctet->setInputEos(pPvt->octetPvt, pasynUser,
                                     eos,eoslen);
}

static asynStatus getInputEos(void *ppvt, asynUser *pasynUser,
    char *eos, int eossize, int *eoslen)
{
    modbusPvt *pPvt = (modbusPvt *)ppvt;
    return pPvt->pasynOctet->getInputEos(pPvt->octetPvt, pasynUser,
                                     eos, eossize, eoslen);
}
static asynStatus setOutputEos(void *ppvt, asynUser *pasynUser,
    const char *eos, int eoslen)
{
    modbusPvt *pPvt = (modbusPvt *)ppvt;
    return pPvt->pasynOctet->setOutputEos(pPvt->octetPvt, pasynUser,
                                     eos,eoslen);
}

static asynStatus getOutputEos(void *ppvt, asynUser *pasynUser,
    char *eos, int eossize, int *eoslen)
{
    modbusPvt *pPvt = (modbusPvt *)ppvt;
    return pPvt->pasynOctet->getOutputEos(pPvt->octetPvt, pasynUser,
                                     eos, eossize, eoslen);
}


/* register modbusInterposeConfig*/
static const iocshArg modbusInterposeConfigArg0 = { "portName", iocshArgString };
static const iocshArg modbusInterposeConfigArg1 = { "link type", iocshArgInt };
static const iocshArg modbusInterposeConfigArg2 = { "timeout (msec)", iocshArgInt };
static const iocshArg modbusInterposeConfigArg3 = { "write delay (msec)", iocshArgInt };
static const iocshArg *modbusInterposeConfigArgs[] = {
                                                    &modbusInterposeConfigArg0,
                                                    &modbusInterposeConfigArg1,
                                                    &modbusInterposeConfigArg2,
                                                    &modbusInterposeConfigArg3};
static const iocshFuncDef modbusInterposeConfigFuncDef =
    {"modbusInterposeConfig", 4, modbusInterposeConfigArgs};
static void modbusInterposeConfigCallFunc(const iocshArgBuf *args)
{
    modbusInterposeConfig(args[0].sval,
                          args[1].ival,
                          args[2].ival,
                          args[3].ival);
}

static void modbusInterposeRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister(&modbusInterposeConfigFuncDef, modbusInterposeConfigCallFunc);
    }
}
epicsExportRegistrar(modbusInterposeRegister);
