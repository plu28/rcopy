/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include <arpa/inet.h>
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
#include "safeUtil.h"

void processClient(int socketNum);
int checkArgs(int argc, char *argv[], float *errRate);

int main(int argc, char *argv[]) {
  int socketNum = 0;
  int portNumber = 0;
  float errRate = 0;

  portNumber = checkArgs(argc, argv, &errRate);
  sendErr_init(errRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

  socketNum = udpServerSetup(portNumber);

  processClient(socketNum);

  close(socketNum);

  return 0;
}

void processClient(int socketNum) {
  int pduLen = 0;
  struct sockaddr_in6 client;
  int clientAddrLen = sizeof(client);

  while (true) {
    pdu recvPdu = pdu();
    pduLen = safeRecvfrom(socketNum, recvPdu.getBuffer().data(),
                          recvPdu.getBuffer().size(), 0,
                          (struct sockaddr *)&client, &clientAddrLen);
    recvPdu.pduResize(pduLen);

    printf("Received message from client with ");
    printIPInfo(&client);
    printf("\nPDU From Client:\n");
    std::cout << recvPdu;

    // Send back the data
    safeSendto(socketNum, recvPdu.getBuffer().data(), pduLen, 0,
               (struct sockaddr *)&client, clientAddrLen);
  }
}

int checkArgs(int argc, char *argv[], float *errRate) {
  // Checks args, returns port number, sets errRate
  int portNumber = 0;

  if (argc > 3 || argc < 2) {
    fprintf(stderr, "Usage %s err-rate [optional port number]\n", argv[0]);
    exit(-1);
  }

  *errRate = atof(argv[1]);
  if (*errRate >= 1 || *errRate < 0) {
    fprintf(stderr, "Error rate must be [0, 1)\n");
    exit(-1);
  }

  if (argc == 3) {
    portNumber = atoi(argv[2]);
  }

  return portNumber;
}
