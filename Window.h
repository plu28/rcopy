#include "pdu.h"
class Window {
public:
  Window(int size);
  ~Window();

  int getSize() const;
  pdu getUpper() const;
  pdu getCurrent() const;
  pdu getLower() const;
  bool isClosed();

  void ack(int ackNum);
  void pushPacket(pdu pduBuffer);
  pdu getPacket(int seqNum);
};
