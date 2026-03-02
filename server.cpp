#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Window.h"
#include "cpe464.h"
#include "gethostbyname.h"
#include "networks.h"
#include "pdu.h"
#include "pollLib.h"

#define RETRY_LIM 10
#define MS_RESEND 1000 // 1 second to resend
enum State {
  FILENAME,
  SEND_DATA,
  WAIT_ON_ACK,
  WAIT_ON_EOF_ACK,
  DONE,
};
static float errRate = 0;

void handleZombies(int sig);
int checkArgs(int argc, char *argv[]);

void processServer(int socketNum);
void processClient(struct sockaddr_in6 *client, pdu &initPDU);
std::ifstream processInitPDU(pdu &initPDU, uint32_t *bufferSize,
                             uint32_t *windowSize);

State checkFilename(int socket, struct sockaddr_in6 *client,
                    std::ifstream &file);
State sendData(int socket, struct sockaddr_in6 *client, std::ifstream &file,
               int bufferSize, Window &w);
State waitOnAck(int socket, struct sockaddr_in6 *client, Window &w);
State waitOnEOF(int socket, struct sockaddr_in6 *client, Window &w);
State handleAcks(int socketNum, struct sockaddr_in6 *client, Window &w,
                 State prev);

int main(int argc, char *argv[]) {
  int socketNum = 0;
  int portNumber = 0;

  portNumber = checkArgs(argc, argv);
  sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

  socketNum = udpServerSetup(portNumber);

  processServer(socketNum);

  close(socketNum);

  return 0;
}

// Server receives a connection and passes it on to a child
void processServer(int socketNum) {
  pid_t pid = 0;
  struct sockaddr_in6 client;
  int clientAddrLen = sizeof(client);
  signal(SIGCHLD, handleZombies);

  while (true) {
    pdu initPDU = pdu(socketNum, &client, &clientAddrLen);
    if (DEBUG) {
      printIPInfo(&client);
      std::cout << initPDU;
    }

    // Just ignore the packet if its already bad or not a client init
    if (!initPDU.badChecksum() || initPDU.flag() != CLIENT_INIT) {
      if ((pid = fork()) < 0) {
        perror("fork");
        exit(-1);
      }

      if (pid == 0) {
        close(socketNum); // Child doesn't need parents socket
        processClient(&client, initPDU);
        exit(0);
      } else {
        std::cout << "\nChild forked with pid: " << pid << std::endl;
      }
    }
  }
}

// Child responsible for handling data
void processClient(struct sockaddr_in6 *client, pdu &initPDU) {
  // Setting up the child
  int socket = safeGetUdpSocket();
  setupPollSet();
  addToPollSet(socket);

  sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

  uint32_t bufferSize = 0;
  uint32_t windowSize = 0;
  std::ifstream file = processInitPDU(initPDU, &bufferSize, &windowSize);
  Window *windowPtr = new Window(windowSize, START_SEQ_NUM);

  State state = FILENAME;
  while (state != DONE) {
    switch (state) {
    case FILENAME:
      state = checkFilename(socket, client, file);
      break;
    case SEND_DATA:
      state = sendData(socket, client, file, bufferSize, *windowPtr);
      break;
    case WAIT_ON_ACK:
      state = waitOnAck(socket, client, *windowPtr);
      break;
    case WAIT_ON_EOF_ACK:
      state = waitOnEOF(socket, client, *windowPtr);
      break;
    case DONE:
      break;
    default:
      printf("Server hit default. One of the functions returned an invalid "
             "state.\n");
      state = DONE;
      break;
    }
  }
  // Cleanup
  close(socket);
  file.close();
  delete windowPtr;
}

// Check if the file is accessible
State checkFilename(int socket, struct sockaddr_in6 *client,
                    std::ifstream &file) {
  if (!file) {
    // Bad file
    pdu badFilenamePDU = pdu(1, 0, SERVER_INIT);
    badFilenamePDU.sendTo(socket, client);
    if (DEBUG) {
      std::cout << "\n\033[91m"
                << "BAD FILENAME"
                << "\033[0m\n"
                << std::endl;
    }
    return DONE;
  } else {
    // Good file
    if (DEBUG) {
      std::cout << "\n\033[92m"
                << "GOOD FILENAME"
                << "\033[0m\n"
                << std::endl;
    }
    pdu goodFilenamePDU = pdu(0, 0, SERVER_INIT);
    goodFilenamePDU.sendTo(socket, client);
    return SEND_DATA;
  }
}

// Send a buffers worth of data
State sendData(int socket, struct sockaddr_in6 *client, std::ifstream &file,
               int bufferSize, Window &w) {
  // Check poll(0) and handle RR/SREJ
  if (pollCall(0) > 0) {
    return handleAcks(socket, client, w, SEND_DATA);
  }
  // Don't send data if the window is closed
  if (w.isClosed()) {
    return WAIT_ON_ACK;
  }
  std::vector<char> buffer(bufferSize);
  file.read(buffer.data(), bufferSize);
  if (file.gcount() > 0) {
    // Send data packet
    pdu dataPDU =
        pdu((uint8_t *)buffer.data(), file.gcount(), w.getCurrent(), DATA);
    dataPDU.sendTo(socket, client);
    w.pushPacket(dataPDU);
  } else {
    if (DEBUG) {
      std::cout << "\n\033[91m"
                << "REACHED EOF"
                << "\033[0m\n"
                << std::endl;
    }
    // Send eof packet
    pdu eofPDU = pdu(0, w.getCurrent(), EOF_FLAG);
    eofPDU.sendTo(socket, client);
    w.pushPacket(eofPDU);
    return WAIT_ON_EOF_ACK;
  }

  // Keep sending data otherwise
  return SEND_DATA;
}

// Waiting on rcopy for an ack during usage phase
State waitOnAck(int socket, struct sockaddr_in6 *client, Window &w) {
  if (!w.isClosed()) {
    if (DEBUG)
      std::cout << "\033[103m" << "\nWINDOW CLOSED\n"
                << "\033[0m\n"
                << std::endl;
    return SEND_DATA;
  }
  for (int retryCount = 0; retryCount < RETRY_LIM; retryCount++) {
    if (pollCall(MS_RESEND) > 0) {
      return handleAcks(socket, client, w, WAIT_ON_ACK);
    } else {
      if (DEBUG)
        std::cout << "\033[103m"
                  << "\n WE TIMED OUT RESENDING LOWEST: " << w.getLowerSeq()
                  << "\033[0m\n"
                  << std::endl;
      // Resend the lowest packet if timeout
      w.getLower().sendTo(socket, client);
      retryCount++;
    }
  }
  return DONE;
}

// NOTE: Should be called by sendData when EOF is read
// The only difference between this function and a regular wait
// is the timeout handling.
State waitOnEOF(int socket, struct sockaddr_in6 *client, Window &w) {
  for (int retryCount = 0; retryCount < RETRY_LIM; retryCount++) {
    if (pollCall(MS_RESEND) > 0) {
      return handleAcks(socket, client, w, WAIT_ON_EOF_ACK);
    } else {
      // Resend EOF packet (last sent PDU)
      w.getLast().sendTo(socket, client);
      retryCount++;
    }
  }
  return DONE;
}

// Update the window if an RR,
// Resend a packet if an SREJ
// Close if EOF ack received
State handleAcks(int socket, struct sockaddr_in6 *client, Window &w,
                 State prev) {
  // Process all of the packets
  while (pollCall(0) > 0) {
    int addrLen = 0;
    pdu recvPDU(socket, client, &addrLen);
    // TODO: Verify checksum
    switch (recvPDU.flag()) {
    case RR:
      if (DEBUG)
        std::cout << "\033[94m" << "\n GOT RR " << recvPDU.payloadInt()
                  << "\033[0m\n"
                  << std::endl;
      w.ack(recvPDU.payloadInt());
      break;
    case SREJ:
      if (DEBUG)
        std::cout << "\033[94m" << "\n GOT SREJ, RESENDING "
                  << recvPDU.payloadInt() << "\033[0m\n"
                  << std::endl;
      w.getPacket(recvPDU.payloadInt()).sendTo(socket, client);
      break;
    case EOF_FLAG:
      return DONE;
      break;
    default:
      if (DEBUG)
        printf("Received unexpected packet with flag %d\n", recvPDU.flag());
      break;
    }
  }
  return prev;
}

int checkArgs(int argc, char *argv[]) {
  // Checks args, returns port number, sets errRate
  int portNumber = 0;

  if (argc > 3 || argc < 2) {
    fprintf(stderr, "Usage %s err-rate [optional port number]\n", argv[0]);
    exit(-1);
  }

  errRate = atof(argv[1]);
  if (errRate >= 1 || errRate < 0) {
    fprintf(stderr, "Error rate must be [0, 1)\n");
    exit(-1);
  }

  if (argc == 3) {
    portNumber = atoi(argv[2]);
  }

  return portNumber;
}

// parse an initPDU format into buffersize and a filestream
std::ifstream processInitPDU(pdu &initPDU, uint32_t *bufferSize,
                             uint32_t *windowSize) {
  // Copy the buffer size
  std::memcpy(bufferSize, initPDU.payload().data(), sizeof(uint32_t));

  // Copy the window size
  std::memcpy(windowSize, initPDU.payload().data() + sizeof(uint32_t),
              sizeof(uint32_t));

  // Copy the filename
  std::string filename;
  int filenameLen = initPDU.payloadLen() - (2 * sizeof(uint32_t));
  filename.resize(filenameLen);
  std::memcpy(&filename[0], initPDU.payload().data() + (2 * sizeof(uint32_t)),
              filenameLen);

  if (DEBUG) {
    std::cout << "\033[31mWINDOW SIZE: " << *windowSize
              << " BUFFER SIZE: " << *bufferSize << " FILENAME: " << filename
              << "\033[0m\n"
              << std::endl;
  }
  std::ifstream file(filename, std::ios::binary);
  return file;
}

void handleZombies(int sig) {
  int stat = 0;
  while (waitpid(-1, &stat, WNOHANG) > 0)
    ;
}
