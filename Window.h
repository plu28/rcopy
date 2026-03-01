#include "pdu.h"
class Window {
public:
  Window(int size);
  ~Window();

  int getSize() const;
  int getUpper() const;
  int getCurrent() const;
  int getLower() const;
  bool isClosed();

  void pushPacket(pdu pduBuffer);
  pdu getPacket(int index);
};
