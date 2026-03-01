// Global variable for holding buffer
// must be malloced
// Get lower
// Get upper
// Get current
// Get window state
// Clients need to store buffers, and retrieve them
#include "Window.h"
#include "pdu.h"
#include <vector>

Window::Window(int size, int startSeq) {
  this->size = size;
  current = startSeq;
  lower = startSeq;
  upper = lower + size;
  window = std::vector<pdu>(size);
}
Window::~Window() { return; }

int Window::getSize() const { return size; }
pdu Window::getUpper() const { return window[upper % size]; }
int Window::getCurrent() const { return current; }
pdu Window::getLower() const { return window[lower % size]; }
pdu Window::getLast() const { return window[window.size() - 1]; }

// Push a packet to the buffer
void Window::pushPacket(pdu packet) {
  if (!this->isClosed()) {
    current++; // The next sequence number to send
    int index = packet.seq() % size;
    window[index] = packet;
  }
};

// Register an acknowledgement (moves window)
void Window::ack(int ackNum) {
  lower = ackNum;
  upper = ackNum + size;
}

bool Window::isClosed() { return current == upper; }

// Get the packet in an index in the window buffer
pdu Window::getPacket(int seqNum) {
  int index = seqNum % size;
  return window[index];
}
