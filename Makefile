CXX = g++
CC= gcc
CFLAGS= -g -Wall -pedantic -std=gnu99
CXXFLAGS= -g -Wall -Wextra -std=c++11

OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o pdu.o Window.o
#uncomment next two lines if your using sendtoErr() library
LIBS += libcpe464.2.21.a -lstdc++ -ldl
CFLAGS += -D__LIBCPE464_


all: rcopy server

rcopy: rcopy.cpp $(OBJS) 
	$(CXX) $(CXXFLAGS) -o rcopy rcopy.cpp $(OBJS) $(LIBS)

server: server.cpp $(OBJS) 
	$(CXX) $(CXXFLAGS) -o server server.cpp  $(OBJS) $(LIBS)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -rf myServer myClient rcopy server *.o *.dSYM




