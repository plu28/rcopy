// Client side - UDP Code
// By Hugh Smith	4/1/2017

#include "pdu.h"
#include <fcntl.h>
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

#include "cpe464.h"
#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"

#define MAX_FILENAMELEN 100
#define MAX_WINDOW 230

typedef struct command_params_t {
  char from_file[MAX_FILENAMELEN];
  char to_file[MAX_FILENAMELEN];
  uint32_t window_size;
  uint32_t buffer_size;
  float error_rate;
  char host_name[MAX_FILENAMELEN];
  uint32_t port;
} command_params;

void talkToServer(int socketNum, struct sockaddr_in6 *server);
int readFromStdin(char *buffer);
void checkArgs(int argc, char *argv[]);

static uint32_t sequenceCounter = 0;
static command_params cp;

// Just set things up
int main(int argc, char *argv[]) {
  int socketNum = 0;
  struct sockaddr_in6 server; // Supports 4 and 6 but requires IPv6 struct
  float errRate = 0;

  checkArgs(argc, argv);

  sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

  socketNum = setupUdpClientToServer(&server, cp.host_name, cp.port);

  talkToServer(socketNum, &server);

  close(socketNum);

  return 0;
}

void talkToServer(int socketNum, struct sockaddr_in6 *server) {
  int serverAddrLen = sizeof(struct sockaddr_in6);
  char *ipString = NULL;
  int payloadLen = 0;
  char payloadBuffer[MAX_BUFFER + 1];
  int recvPDULen = 0;

  payloadBuffer[0] = '\0';
  while (payloadBuffer[0] != '.') {
    payloadLen = readFromStdin(payloadBuffer);

    printf("Sending: %s with len: %d\n", payloadBuffer, payloadLen);
    pdu newPdu((uint8_t *)payloadBuffer, payloadLen, sequenceCounter++, 1);

    safeSendto(socketNum, newPdu.buffer().data(), newPdu.PDULen(), 0,
               (struct sockaddr *)server, serverAddrLen);

    pdu recvPdu = pdu();
    recvPDULen = safeRecvfrom(socketNum, recvPdu.buffer().data(),
                              recvPdu.buffer().size(), 0,
                              (struct sockaddr *)server, &serverAddrLen);
    recvPdu.resize(recvPDULen);

    // print out bytes received
    ipString = ipAddressToString(server);
    printf("Server with ip: %s and port %d said it received a pdu\n\nPDU From "
           "Server:\n",
           ipString, ntohs(server->sin6_port));
    std::cout << recvPdu;
  }
}

int readFromStdin(char *buffer) {
  char aChar = 0;
  int inputLen = 0;

  // Important you don't input more characters than you have space
  buffer[0] = '\0';
  printf("Enter data: ");
  while (inputLen < (MAX_BUFFER - 1) && aChar != '\n') {
    aChar = getchar();
    if (aChar != '\n') {
      buffer[inputLen] = aChar;
      inputLen++;
    }
  }

  // Null terminate the string
  buffer[inputLen] = '\0';
  inputLen++;

  return inputLen;
}

void checkArgs(int argc, char *argv[]) {

  // check arg amount
  if (argc != 8) {
    printf("usage: %s [from-filename] [to-filename] [window-size] "
           "[buffer-size] [error-rate] [host-name] [port-number] \n",
           argv[0]);
    exit(1);
  }

  // Assign variables
  strncpy(cp.from_file, argv[1], MAX_FILENAMELEN);
  strncpy(cp.to_file, argv[2], MAX_FILENAMELEN);

  char *end;
  cp.window_size = (uint32_t)strtol(argv[3], &end, 10);
  cp.buffer_size = (uint32_t)strtol(argv[4], &end, 10);
  cp.error_rate = (float)strtol(argv[5], &end, 10);
  strncpy(cp.host_name, argv[6], MAX_FILENAMELEN);
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
  if (cp.error_rate >= 1 || cp.error_rate < 0) {
    fprintf(stderr, "Error rate must be [0, 1)\n");
    exit(-1);
  }
  if (cp.buffer_size < 1 || cp.buffer_size > 1400) {
    fprintf(stderr, "Buffer size must be [1, %d]\n", MAX_BUFFER);
    exit(-1);
  }
  if (cp.window_size < 1 || cp.window_size > 1400) {
    fprintf(stderr, "Window size must be [1, %d]\n", MAX_WINDOW);
    exit(-1);
  }
}
