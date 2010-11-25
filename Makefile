CPPFLAGS+=-O2 -I.
LDFLAGS+=-levent_core -lstdc++

all: swift

swift: swift.o sha1.o compat.o sendrecv.o send_control.o hashtree.o bin64.o bins.o channel.o transfer.o

