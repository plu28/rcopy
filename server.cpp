/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

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

#define START_SEQ_NUM 200
#define RETRY_LIM 10
enum State {
  FILENAME,
  SEND_DATA,
  WAIT_ON_ACK,
  TIMEOUT_ON_ACK,
  WAIT_ON_EOF_ACK,
  TIMEOUT_ON_EOF_ACK,
  DONE,
};
static float errRate = 0;

void processServer(int socketNum);
void processClient(struct sockaddr_in6 *client, pdu initPDU);
std::ifstream processInitPDU(pdu initPDU, int *bufferSize);
void handleZombies(int sig);
int checkArgs(int argc, char *argv[]);

State checkFilename(int socket, struct sockaddr_in6 *client,
                    std::ifstream &file, int *seq_num);
State sendData(int socket, struct sockaddr_in6 *client, std::ifstream &file,
               int *seqNum, int bufferSize);

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
void processClient(struct sockaddr_in6 *client, pdu initPDU) {
  // Get a new socket and setup error
  int socket = safeGetUdpSocket();
  sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
  int bufferSize = 0;
  std::ifstream file = processInitPDU(initPDU, &bufferSize);

  State state = FILENAME;
  int seqNum = START_SEQ_NUM;
  while (state != DONE) {
    switch (state) {
    case FILENAME:
      state = checkFilename(socket, client, file, &seqNum);
      break;
    case SEND_DATA:
      state = sendData(socket, client, file, &seqNum, bufferSize);
      break;
    case WAIT_ON_ACK:
      break;
    case TIMEOUT_ON_ACK:
      break;
    case WAIT_ON_EOF_ACK:
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
               int *seqNum, int bufferSize) {}

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
std::ifstream processInitPDU(pdu initPDU, int *bufferSize) {
  std::memset(bufferSize, initPDU.payloadInt(), sizeof(int));
  std::string filename;
  int filenameLen = initPDU.payloadLen() - sizeof(int);
  filename.resize(filenameLen);
  std::memcpy(&filename[0], initPDU.payload().data() + sizeof(int),
              filenameLen);
  // std::cout << "FILENAME: " << filename << "| FILENAME LEN: " << filenameLen
  //           << std::endl;
  std::ifstream file(filename, std::ios::binary);
  return file;
}

void handleZombies(int sig) {
  int stat = 0;
  while (waitpid(-1, &stat, WNOHANG) > 0)
    ;
}
