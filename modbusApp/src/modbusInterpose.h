/* modbusInterpose.h */
/* 
 * Interpose interface support for Modbus with TCP/IP or serial
 *
 * Author: Mark Rivers
 */

#ifndef modbusInterpose_H
#define modbusInterpose_H

#include <shareLib.h>
#include <epicsExport.h>

typedef enum {
    modbusLinkTCP,
    modbusLinkRTU,
    modbusLinkASCII
} modbusLinkType;

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

epicsShareFunc int modbusInterposeConfig(const char *portName, 
                                         modbusLinkType linkType, 
                                         int timeoutMsec, int writeDelayMsec);
#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* modbusInterpose_H */
