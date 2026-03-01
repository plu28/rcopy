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
  Window(int size, int startSeq) {
    this->size = size;
    current = startSeq;
    lower = startSeq;
    upper = lower + size;
    window = std::vector<pdu>(size);
  }
  ~Window();

  int getSize() { return size; }
  pdu getUpper() { return window[upper % size]; }
  int getCurrent() { return current; }
  pdu getLower() { return window[lower % size]; }
  pdu getLast() { return window[window.size() - 1]; }

  // Push a packet to the buffer
  void pushPacket(pdu packet) {
    if (!this->isClosed()) {
      // BUG: Maybe? Current is a sequence number
      current = packet.seq() + 1; // The next sequence number to send
      int index = packet.seq() % size;
      window[index] = packet;
    }
  };

  // Register an acknowledgement (moves window)
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
