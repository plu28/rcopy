// PDU class
// Used for making PDUs, printing PDUs, and receiving PDUs

#include "pdu.h"
#include "checksum.h"
#include "gethostbyname.h"
#include "safeUtil.h"
#include <iostream>
#include <netinet/in.h>
#include <ostream>
#include <stdint.h>
#include <vector>
#define MAX_PDU 1407

// Creating a pdu based on data from a socket
pdu::pdu(int socket, struct sockaddr_in6 *source, int *addrLen) {
  int recvPDULen = safeRecvfrom(socket, this->pduBuffer.data(), this->PDULen(),
                                0, (struct sockaddr *)source, addrLen);
  pduBuffer.resize(recvPDULen);
  if (DEBUG) {
    printf("RECEIVED A NEW PDU FROM ");
    printIPInfo(source);
    std::cout << *this << std::endl;
  }
}

// PDU with a regular payload
pdu::pdu(uint8_t *payload, int payloadLen, uint32_t seq_num, uint8_t flag) {
  pduBuffer = std::vector<uint8_t>(payloadLen + PDU_HEADER_LEN);
  // Build the pdu buffer
  uint32_t seq_num_net = htonl(seq_num);
  std::memcpy(pduBuffer.data() + SEQ_OFFSET, &seq_num_net, sizeof(uint32_t));

  std::memset(pduBuffer.data() + CHK_OFFSET, 0x00, sizeof(uint16_t));

  std::memcpy(pduBuffer.data() + FLAG_OFFSET, &flag, sizeof(uint8_t));

  std::memcpy(pduBuffer.data() + PAYLOAD_OFFSET, payload, payloadLen);

  uint16_t checksum = in_cksum((uint16_t *)pduBuffer.data(), pduBuffer.size());
  std::memcpy(pduBuffer.data() + CHK_OFFSET, &checksum, sizeof(uint16_t));
  if (DEBUG)
    std::cout << "\n\033[92m" << "\nCREATED PDU WITH REGULAR PAYLOAD\n"
              << "\033[0m\n"
              << *this << std::endl;
}
// PDU with an integer payload (for SREJ, RR, and init response)
pdu::pdu(int payload, uint32_t seq_num, uint8_t flag) {
  pduBuffer = std::vector<uint8_t>(sizeof(int) + PDU_HEADER_LEN);
  // Build the pdu buffer
  uint32_t seq_num_net = htonl(seq_num);
  std::memcpy(pduBuffer.data() + SEQ_OFFSET, &seq_num_net, sizeof(uint32_t));

  std::memset(pduBuffer.data() + CHK_OFFSET, 0x00, sizeof(uint16_t));

  std::memcpy(pduBuffer.data() + FLAG_OFFSET, &flag, sizeof(uint8_t));

  std::memcpy(pduBuffer.data() + PAYLOAD_OFFSET, &payload, sizeof(int));

  uint16_t checksum = in_cksum((uint16_t *)pduBuffer.data(), pduBuffer.size());
  std::memcpy(pduBuffer.data() + CHK_OFFSET, &checksum, sizeof(uint16_t));
  if (DEBUG)
    std::cout << "\n\033[92m" << "\nCREATED PDU WITH INTEGER PAYLOAD\n"
              << "\033[0m\n"
              << *this << std::endl;
}

// Returns 1 if checksum fails
int pdu::badChecksum() const {
  uint16_t cksumVal = in_cksum((uint16_t *)pduBuffer.data(), pduBuffer.size());
  if (cksumVal == 0) {
    return 0;
  }
  return 1;
}

uint32_t pdu::seq() const {
  uint32_t seq_num = 0;
  std::memcpy(&seq_num, pduBuffer.data() + SEQ_OFFSET, sizeof(uint32_t));
  seq_num = ntohl(seq_num);
  return seq_num;
}
int pdu::flag() const {
  int flag = 0;
  std::memcpy(&flag, pduBuffer.data() + FLAG_OFFSET, sizeof(uint8_t));
  return flag;
}

uint16_t pdu::checksum() const {
  uint16_t checksum = 0;
  std::memcpy(&checksum, pduBuffer.data() + CHK_OFFSET, sizeof(uint16_t));
  return checksum;
}
int pdu::payloadLen() const { return pduBuffer.size() - PDU_HEADER_LEN; }
int pdu::PDULen() const { return pduBuffer.size(); }

std::vector<uint8_t> pdu::payload() const {
  std::vector<uint8_t> payload = std::vector<uint8_t>(this->payloadLen());
  memcpy(payload.data(), pduBuffer.data() + PAYLOAD_OFFSET, payloadLen());
  return payload;
}

// Interpret the payload as a string
std::string pdu::payloadStr() const {
  std::vector<uint8_t> payload = this->payload();
  return std::string(payload.begin(), payload.end());
}

// Interpret the payload as an integer
int pdu::payloadInt() const {
  std::vector<uint8_t> payload = this->payload();
  int payloadInt = 0;
  std::memcpy(&payloadInt, payload.data(), sizeof(int));
  return payloadInt;
}

std::vector<uint8_t> &pdu::buffer() { return pduBuffer; }

void pdu::resize(int pduLen) { pduBuffer.resize(pduLen); }

int pdu::sendTo(int socketNum, struct sockaddr_in6 *destination) {
  if (DEBUG) {
    printf("SENDING PDU TO");
    printIPInfo(destination);
    std::cout << *this << std::endl;
    ;
  }
  int addrLen = sizeof(struct sockaddr_in6);
  return safeSendto(socketNum, this->pduBuffer.data(), this->PDULen(), 0,
                    (struct sockaddr *)destination, addrLen);
}

// Print overload
std::ostream &operator<<(std::ostream &os, const pdu &pdu) {
  std::string packetType;
  switch (pdu.flag()) {
  case (RR):
    packetType = "RR";
    break;
  case (SREJ):
    packetType = "SREJ";
    break;
  case (CLIENT_INIT):
    packetType = "CLIENT INIT";
    break;
  case (SERVER_INIT):
    packetType = "SERVER INIT";
    break;
  case (EOF_FLAG):
    packetType = "EOF";
    break;
  case (DATA):
    packetType = "DATA";
    break;
  case (DATA_SREJ):
    packetType = "DATA SREJ";
    break;
  case (DATA_TIMEOUT):
    packetType = "DATA TIMEOUT";
    break;
  default:
    packetType = "Default";
    break;
  }
  os << "Checksum: " << (pdu.badChecksum() ? "Fail" : "Pass") << " | ";
  os << "Sequence Number: " << (pdu.seq()) << " | ";
  os << "Flag: " << packetType << " (" << pdu.flag() << ")" << " | ";
  os << "PDU Length: " << (pdu.PDULen()) << " | ";
  os << "Payload Length: " << (pdu.payloadLen()) << " | \n";
  for (uint8_t byte : pdu.payload()) {
    os << static_cast<int>(byte) << " ";
  }
  return os;
}
