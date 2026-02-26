#pragma once

#include <cstdint>
#include <vector>

// PDU Flags
enum Flag {
  RR = 5,
  SREJ = 6,
  CLIENT_INIT = 8,
  SERVER_INIT = 9,
  EOF_FLAG = 10,
  DATA = 16,
  DATA_SREJ = 17,
  DATA_TIMEOUT = 18
};

#define MAX_PDU 1407
#define MAX_BUFFER 1400
#define PDU_HEADER_LEN 7
#define SEQ_OFFSET 0
#define CHK_OFFSET 4
#define FLAG_OFFSET 6
#define PAYLOAD_OFFSET 7
class pdu {
public:
  pdu();
  pdu(uint8_t *pduBuffer);
  pdu(uint8_t *payload, int payloadLen, uint32_t seq_num, uint8_t flag);

  uint32_t seq() const;
  int flag() const;
  uint16_t checksum() const;
  std::vector<uint8_t> payload() const;
  std::vector<uint8_t> &buffer();

  int payloadLen() const;
  int PDULen() const;
  std::string payloadStr() const;

  int badChecksum() const;
  void resize(int payloadLen);
  int sendTo(int socketNum, struct sockaddr *destination);
  int recvFrom(int socketNum, struct sockaddr *source, int *addrLen);

private:
  std::vector<uint8_t> pduBuffer = std::vector<uint8_t>(MAX_PDU);
};

std::ostream &operator<<(std::ostream &os, const pdu &pdu);
