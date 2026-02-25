
#ifndef PDUUTIL_H
#define PDUUTIL_H
#define PDU_HEADER_LEN 7

#include <stdint.h>
int createPDU(uint8_t *pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t *payload, int payloadLen); 
void printPDU(uint8_t *aPDU, int pduLength);


#endif  
