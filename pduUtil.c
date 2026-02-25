#include "pduUtil.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include "checksum.h"

int createPDU(uint8_t *pduBuffer, uint32_t sequenceNumber, uint8_t flag,
              uint8_t *payload, int payloadLen) {
	uint8_t *pduBufferPtr = pduBuffer;
	int totalLen = 0;

	// Append the sequence number
	uint32_t seqNumNet = htonl(sequenceNumber);
	memcpy(pduBufferPtr, &seqNumNet, sizeof(uint32_t));
	pduBufferPtr += sizeof(uint32_t);
	totalLen += sizeof(uint32_t);

	// Append the checksum
	uint8_t *ckSumPtr = pduBufferPtr;
	memset(pduBufferPtr, 0x00, sizeof(uint16_t)); // Set it to 0 for now
	pduBufferPtr += sizeof(uint16_t);
	totalLen += sizeof(uint16_t);

	// Append the flag
	memcpy(pduBufferPtr, &flag, sizeof(uint8_t));
	pduBufferPtr += sizeof(uint8_t);
	totalLen += sizeof(uint8_t);

	// Append the payload
	memcpy(pduBufferPtr, payload, payloadLen);
	totalLen += payloadLen;

	// Computing and setting checksum value
	uint16_t cksumVal = in_cksum((uint16_t*)pduBuffer, totalLen);
	memcpy(ckSumPtr, &cksumVal, sizeof(uint16_t));

	return totalLen;
}

void printPDU(uint8_t *aPDU, int pduLength) {
	uint8_t *aPDUPtr = aPDU;
	int pduHeaderLen = 0;
	
	// Verify checksum
	uint16_t cksumVal = in_cksum((uint16_t*)aPDU, pduLength);
	if (cksumVal == 0) {
		printf("Checksum result: Pass\n");
	}
	if (cksumVal != 0) {
		fprintf(stderr, "Checksum result: Fail\n");
	}
	
	// Get sequence number
	uint32_t seqNumNet = 0;
	memcpy(&seqNumNet, aPDUPtr, sizeof(uint32_t));
	uint32_t seqNumHost = ntohl(seqNumNet);
	aPDUPtr += sizeof(uint32_t);
	pduHeaderLen += sizeof(uint32_t);
	printf("Sequence Number: %d\n", seqNumHost);

	aPDUPtr += sizeof(uint16_t); // Skip over checksum bytes 
	pduHeaderLen += sizeof(uint16_t);
	
	// Get flag
	uint8_t flag = -1;
	memcpy(&flag, aPDUPtr, sizeof(uint8_t));
	aPDUPtr += sizeof(uint8_t);
	pduHeaderLen += sizeof(uint8_t);
	printf("Flag: %u\n", flag);

	// Payload length 
	int payloadLength = pduLength - pduHeaderLen;
	printf("Payload Length: %d\n", payloadLength);

	// Payload
	printf("Payload: %s\n", aPDUPtr);
	

}
