#include "pdu.h"
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "Window.h"
#include "cpe464.h"
#include "networks.h"
#include "pollLib.h"

#define MAX_FILENAMELEN 100
#define INIT_PAYLOAD_LEN 108
#define MAX_WINDOW 230
#define RETRY_LIM 10
#define MS_RESEND 1000     // 1 second to resend
#define MS_TERMINATE 10000 // 10 seconds to die

typedef struct command_params_t {
  char fromFile[MAX_FILENAMELEN];
  char toFile[MAX_FILENAMELEN];
  uint32_t windowSize;
  uint32_t bufferSize;
  float errorRate;
  char hostName[MAX_FILENAMELEN];
  uint32_t port;
} command_params;

enum State { CONNECT, RECV_DATA, TIMEOUT, BUFFER_DATA, DONE };

// int readFromStdin(char *buffer);
void checkArgs(int argc, char *argv[]);
void processFile();
State establishConnection(int socketNum, sockaddr_in6 *server,
                          std::ofstream &outfile, Window *&w);

State recvData(int socketNum, sockaddr_in6 *server, std::ofstream &outfile,
               Window *&w);

static command_params cp;

int main(int argc, char *argv[]) {

  checkArgs(argc, argv);

  processFile();

  return 0;
}

void processFile() {
  struct sockaddr_in6 server; // Supports 4 and 6 but requires IPv6 struct
  int socketNum = setupUdpClientToServer(&server, cp.hostName, cp.port);

  setupPollSet();
  addToPollSet(socketNum);
  sendErr_init(cp.errorRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
  std::ofstream outfile;
  Window *w = nullptr;

  State state = CONNECT;
  while (state != DONE) {
    switch (state) {
    case CONNECT:
      state = establishConnection(socketNum, &server, outfile, w);
      break;
    case RECV_DATA:
      state = recvData(socketNum, &server, outfile, w);
      break;
    case DONE:
      break;
    case TIMEOUT:
      printf("Connection timed out\n");
      state = DONE;
    default:
      printf("Unexpected state in rcopy. (Something went wrong)\n");
      state = DONE;
      break;
    }
  }
  close(socketNum);
  outfile.close();
}

State establishConnection(int socketNum, sockaddr_in6 *server,
                          std::ofstream &outfile, Window *&w) {
  uint8_t payload[INIT_PAYLOAD_LEN];
  // Set the buffer size
  std::memcpy(payload, &cp.bufferSize, sizeof(uint32_t));
  // Set the window size
  std::memcpy(payload + sizeof(uint32_t), &cp.windowSize, sizeof(uint32_t));
  // Set the filename
  uint32_t payloadLen =
      strnlen(cp.fromFile, MAX_FILENAMELEN) + (2 * sizeof(uint32_t));
  std::memcpy(payload + (2 * sizeof(uint32_t)), cp.fromFile, payloadLen);

  for (int retryCount = 0; retryCount < RETRY_LIM; retryCount++) {
    pdu initPDU = pdu(payload, payloadLen, 0, CLIENT_INIT);
    initPDU.sendTo(socketNum, server);
    if (pollCall(MS_RESEND) > 0) {
      // Received a response
      int addrLen = 0;
      pdu initResponse = pdu(socketNum, server, &addrLen);

      // Throw it away if its not good
      if (!initResponse.badChecksum() || initResponse.flag() == SERVER_INIT) {
        // If the payload had a 1, its a bad filename, if 0, its good
        int badFilename = initResponse.payloadInt();
        if (badFilename) {
          printf("%s: No such file on server\n", cp.fromFile);
          return DONE;
        }
        outfile = std::ofstream(cp.toFile, std::ios::binary);
        return RECV_DATA;
      }
    }
  }
  return TIMEOUT;
}

State recvData(int socket, sockaddr_in6 *server, std::ofstream &outfile) {
  // Wait for 10 seconds
  if (pollCall(MS_TERMINATE) > 0) {
    // TODO: Receive the data
  } else {
    // Server has been quiet for 10s, assume its dead
    return TIMEOUT;
  }
}

// int readFromStdin(char *buffer) {
//   char aChar = 0;
//   int inputLen = 0;
//
//   // Important you don't input more characters than you have space
//   buffer[0] = '\0';
//   printf("Enter data: ");
//   while (inputLen < (MAX_BUFFER - 1) && aChar != '\n') {
//     aChar = getchar();
//     if (aChar != '\n') {
//       buffer[inputLen] = aChar;
//       inputLen++;
//     }
//   }
//
//   // Null terminate the string
//   buffer[inputLen] = '\0';
//   inputLen++;
//
//   return inputLen;
// }

void checkArgs(int argc, char *argv[]) {

  // check arg amount
  if (argc != 8) {
    printf("usage: %s [from-filename] [to-filename] [window-size] "
           "[buffer-size] [error-rate] [host-name] [port-number] \n",
           argv[0]);
    exit(1);
  }

  // Assign variables
  strncpy(cp.fromFile, argv[1], MAX_FILENAMELEN - 1);
  strncpy(cp.toFile, argv[2], MAX_FILENAMELEN - 1);

  char *end;
  cp.windowSize = (uint32_t)strtol(argv[3], &end, 10);
  cp.bufferSize = (uint32_t)strtol(argv[4], &end, 10);
  cp.errorRate = (float)strtol(argv[5], &end, 10);
  strncpy(cp.hostName, argv[6], MAX_FILENAMELEN);
  cp.port = (uint32_t)strtol(argv[7], &end, 10);

  // check arg limits
  if (strnlen(argv[1], MAX_FILENAMELEN + 1) > MAX_FILENAMELEN) {
    fprintf(stderr, "Source filename too long\n");
    exit(-1);
  }
  if (strnlen(argv[2], MAX_FILENAMELEN + 1) > MAX_FILENAMELEN) {
    fprintf(stderr, "Destination filename too long\n");
    exit(-1);
  }
  if (strnlen(argv[6], MAX_FILENAMELEN + 1) > MAX_FILENAMELEN) {
    fprintf(stderr, "Host string too long\n");
    exit(-1);
  }
  if (cp.errorRate >= 1 || cp.errorRate < 0) {
    fprintf(stderr, "Error rate must be [0, 1)\n");
    exit(-1);
  }
  if (cp.bufferSize < 1 || cp.bufferSize > 1400) {
    fprintf(stderr, "Buffer size must be [1, %d]\n", MAX_BUFFER);
    exit(-1);
  }
  if (cp.windowSize < 1 || cp.windowSize > 1400) {
    fprintf(stderr, "Window size must be [1, %d]\n", MAX_WINDOW);
    exit(-1);
  }
}
