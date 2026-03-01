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

#include "cpe464.h"
#include "gethostbyname.h"
#include "networks.h"
#include "pdu.h"
#include "pollLib.h"
#include "window.h"

#define START_SEQ_NUM 200
#define RETRY_LIM 10
#define MS_RESEND 1000 // 1 second to resend
enum State {
  FILENAME,
  SEND_DATA,
  WAIT_ON_ACK,
  HANDLE_ACK,
  TIMEOUT_ON_ACK,
  WAIT_ON_EOF_ACK,
  TIMEOUT_ON_EOF_ACK,
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
                    std::ifstream &file, int *seqNum);
State sendData(int socket, struct sockaddr_in6 *client, std::ifstream &file,
               int *seqNum, int bufferSize, Window &w);
State waitOnACK(int socket, struct sockaddr_in6 *client);
State waitOnEOF(int socket, struct sockaddr_in6 *client);
State handleAcks(int socketNum, struct sockaddr_in6 *client, Window &w);

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
  int pduLen = 0;
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
  Window *windowPtr = new Window(windowSize);

  State state = FILENAME;
  int seqNum = START_SEQ_NUM;
  while (state != DONE) {
    switch (state) {
    case FILENAME:
      state = checkFilename(socket, client, file, &seqNum);
      break;
    case SEND_DATA:
      state = sendData(socket, client, file, &seqNum, bufferSize, *windowPtr);
      break;
    case HANDLE_ACK:
      break;
    case WAIT_ON_ACK:
      state = waitOnACK(socket, client);
      break;
    case TIMEOUT_ON_ACK:
      break;
    case WAIT_ON_EOF_ACK:
      state = waitOnEOF(socket, client);
      break;
    case TIMEOUT_ON_EOF_ACK:
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
                    std::ifstream &file, int *seqNum) {
  if (!file) {
    // Bad file
    printf("BAD FILE\n");
    pdu badFilenamePDU = pdu(1, (*seqNum)++, SERVER_INIT);
    badFilenamePDU.sendTo(socket, client);
    return DONE;
  } else {
    // Good file
    printf("GOOD FILE\n");
    pdu goodFilenamePDU = pdu(0, (*seqNum)++, SERVER_INIT);
    goodFilenamePDU.sendTo(socket, client);
    return SEND_DATA;
  }
}

// Send a buffers worth of data
State sendData(int socket, struct sockaddr_in6 *client, std::ifstream &file,
               int *seqNum, int bufferSize, Window &w) {
  // Check poll(0) and handle RR/SREJ
  if (pollCall(0) > 0) {
    return HANDLE_ACK;
  }
  // Don't send data if the window is closed
  if (w.isClosed()) {
    return WAIT_ON_ACK;
  }
  char buffer[bufferSize];
  file.read(buffer, bufferSize);

  if (file.gcount() > 0) {
    // Send data packet
    pdu dataPDU = pdu((uint8_t *)&buffer, file.gcount(), (*seqNum)++, DATA);
    dataPDU.sendTo(socket, client);
  } else {
    // Send eof packet
    pdu eofPDU = pdu(0, (*seqNum++), EOF_FLAG);
    eofPDU.sendTo(socket, client);
    return WAIT_ON_EOF_ACK;
  }

  // Keep sending data otherwise
  return SEND_DATA;
}

// Waiting on rcopy for an ack during usage phase
State waitOnACK(int socket, struct sockaddr_in6 *client) {
  for (int retryCount = 0; retryCount < RETRY_LIM; retryCount++) {
    if (pollCall(MS_RESEND) > 0) {
      // TODO: Received something, so process it
    } else {
      // TODO: Resend the lowest packet in the window
      retryCount++;
    }
  }
  return DONE;
}

// Update the window if an RR,
// Resend a packet if an SREJ
// Close if EOF ack received
State handleAcks(int socket, struct sockaddr_in6 *client, Window &w) {
  // Process all of the packets
  while (pollCall(0) > 0) {
    int addrLen = 0;
    pdu recvPDU(socket, client, &addrLen);
    switch (recvPDU.flag()) {
    case RR:
      // TODO: Update the window
      return SEND_DATA;
      break;
    case SREJ:
      // TODO: Resend the packet
      w.getPacket(w.getLower());
      return SEND_DATA;
      break;
    case EOF_FLAG:
      // TODO: Teardown
      return DONE;
      break;
    default:
      if (DEBUG)
        printf("Received unexpected packet with flag %d\n", recvPDU.flag());
      break;
    }
  }
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

  std::cout << "\033[31mWINDOW SIZE: " << *windowSize
            << " BUFFER SIZE: " << *bufferSize << " FILENAME: " << filename
            << "\033[0m\n"
            << std::endl;
  std::ifstream file(filename, std::ios::binary);
  return file;
}

void handleZombies(int sig) {
  int stat = 0;
  while (waitpid(-1, &stat, WNOHANG) > 0)
    ;
}
