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
  int getLowerSeq() const;
  int getCurrentSeq() const;
  bool isClosed();

  void ack(int ackNum);
  void pushPacket(pdu packet);
  pdu getPacket(int seqNum);

private:
  int lower;
  int upper;
  int current;
  int size;
  std::vector<pdu> window;
};
