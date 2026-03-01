// Global variable for holding buffer
// must be malloced
// Get lower
// Get upper
// Get current
// Get window state
// Clients need to store buffers, and retrieve them
#include "pdu.h"
#include <iostream>
#include <vector>

class Window {
public:
  Window(int size) {
    this->size = size;
    current = 0;
    lower = 0;
    upper = 0 + size;
    window = std::vector<pdu>(size);
  }
  ~Window();

  int getSize() { return size; }
  int getUpper() { return upper; }
  int getCurrent() { return current; }
  int getLower() { return lower; }

  // Push a packet to the buffer
  void pushPacket(pdu packet) {
    if (!this->isClosed()) {
      int index = packet.seq() % size;
      window[index] = packet;
    }
  };

  // Register an acknowledgement
  void ack(int ackNum) {
    lower = ackNum;
    upper = ackNum + size;
  }

  bool isClosed() { return current == upper; }

  // Get the packet in an index in the window buffer
  pdu getPacket(int seqNum) {
    int index = seqNum % size;
    return window[index];
  }

private:
  int lower;
  int upper;
  int current;
  int size;
  std::vector<pdu> window;
};
