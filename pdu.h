#pragma once

#include <cstdint>
#include <vector>
#define RR 5
#define SREJ 6
#define CLIENT_INIT 8
#define SERVER_INIT 9
#define EOF_FLAG 10
#define DATA 16
#define DATA_SREJ 17
#define DATA_TIMEOUT 18

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

private:
  std::vector<uint8_t> pduBuffer = std::vector<uint8_t>(MAX_PDU);
};

std::ostream &operator<<(std::ostream &os, const pdu &pdu);
