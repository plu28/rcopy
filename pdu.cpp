// PDU class
// Used for making PDUs, printing PDUs, and receiving PDUs

#include "pdu.h"
#include "checksum.h"
#include "safeUtil.h"
#include <netinet/in.h>
#include <ostream>
#include <stdint.h>
#include <vector>
#define MAX_PDU 1407

// Create a blank PDU object
pdu::pdu() {}

// Create a pdu object
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

std::string pdu::payloadStr() const {
  std::vector<uint8_t> payload = this->payload();
  return std::string(payload.begin(), payload.end());
}
std::vector<uint8_t> &pdu::buffer() { return pduBuffer; }

void pdu::resize(int pduLen) { pduBuffer.resize(pduLen); }

int pdu::sendTo(int socketNum, struct sockaddr *destination) {
  int addrLen = sizeof(struct sockaddr_in6);
  return safeSendto(socketNum, this->pduBuffer.data(), this->PDULen(), 0,
                    destination, addrLen);
}

int pdu::recvFrom(int socketNum, struct sockaddr *source, int *addrLen) {
  int recvPDULen = safeRecvfrom(socketNum, this->pduBuffer.data(),
                                this->PDULen(), 0, source, addrLen);
  pduBuffer.resize(recvPDULen);
  return recvPDULen;
}

// Print overload
std::ostream &operator<<(std::ostream &os, const pdu &pdu) {
  os << "Checksum: " << (pdu.badChecksum() ? "Fail" : "Pass") << " | ";
  os << "Sequence Number: " << (pdu.seq()) << " | ";
  os << "Flag: " << (pdu.flag()) << " | ";
  os << "PDU Length: " << (pdu.PDULen()) << " | ";
  os << "Payload Length: " << (pdu.payloadLen()) << " | \n";
  os << (pdu.payloadStr()) << "\n";
  return os;
}
