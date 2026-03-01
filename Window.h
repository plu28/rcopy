#include "pdu.h"
class Window {
public:
  Window(int size, int startSeq);
  ~Window();

  int getSize() const;
  pdu getUpper() const;
  int getCurrent() const;
  pdu getLast() const; // Get the last sent PDU
  pdu getLower() const;
  bool isClosed();

  void ack(int ackNum);
  void pushPacket(pdu pduBuffer);
  pdu getPacket(int seqNum);
};
