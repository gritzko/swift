CPPFLAGS=-O2 -I.

all: swift

swift: swift.o sha1.o compat.o sendrecv.o send_control.o hashtree.o bin64.o bins.o channel.o datagram.o transfer.o httpgw.o
	g++ -I. *.o -o swift

