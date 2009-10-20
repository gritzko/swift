/*
 *  datagram.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/9/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include <iostream>

#ifdef _MSC_VER
    #include <winsock2.h>
    typedef int socklen_t;
#else
    #include <arpa/inet.h>
#endif
#include <glog/logging.h>
#include "datagram.h"

namespace p2tp {

tint Datagram::now = Datagram::Time();
uint32_t Datagram::Address::LOCALHOST = INADDR_LOOPBACK;

int Datagram::Send () {
	int r = sendto(sock,(const char *)buf+offset,length-offset,0,
				   (struct sockaddr*)&(addr.addr),sizeof(struct sockaddr_in));
	offset=0;
	length=0;
	now = Time();
	return r;
}

int Datagram::Recv () {
	socklen_t addrlen = sizeof(struct sockaddr_in);
	offset = 0;
	length = recvfrom (sock, (char *)buf, MAXDGRAMSZ, 0,
					   (struct sockaddr*)&(addr), &addrlen);
	if (length<0)
#ifdef _MSC_VER
		PLOG(ERROR)<<"on recv" << WSAGetLastError() << "\n";
#else
		PLOG(ERROR)<<"on recv";
#endif
	now = Time();
	return length;
}


SOCKET Datagram::Wait (int sockcnt, SOCKET* sockets, tint usec) {
	// ARNOTODO: LOG commented out, it causes a crash on win32 (in a strlen()
	// done as part of a std::local::name() ??
	//
	//LOG(INFO)<<"waiting for "<<sockcnt;
	struct timeval timeout;
	timeout.tv_sec = usec/TINT_SEC;
	timeout.tv_usec = usec%TINT_SEC;
	int max_sock_fd = 0;
	fd_set bases, err;
	FD_ZERO(&bases);
	FD_ZERO(&err);
	for(int i=0; i<sockcnt; i++) {
		FD_SET(sockets[i],&bases);
		FD_SET(sockets[i],&err);
		if (sockets[i]>max_sock_fd)
			max_sock_fd = sockets[i];
	}
	int sel = select(max_sock_fd+1, &bases, NULL, &err, &timeout);
	if (sel>0) {
		for (int i=0; i<=sockcnt; i++)
			if (FD_ISSET(sockets[i],&bases))
				return sockets[i];
	} else if (sel<0)
#ifdef _MSC_VER
		PLOG(ERROR)<<"select fails" << WSAGetLastError() << "\n";
#else
		PLOG(ERROR)<<"select fails";
#endif

	// Arno: may return 0 when timeout expired
	return sel;
}

tint Datagram::Time () {
	HiResTimeOfDay* tod = HiResTimeOfDay::Instance();
	tint ret = tod->getTimeUSec();
	//DLOG(INFO)<<"now is "<<ret;
	return now=ret;
}

SOCKET Datagram::Bind (Address addr_) {
    struct sockaddr_in addr = addr_;
	SOCKET fd;
	int len = sizeof(struct sockaddr_in), sndbuf=1<<20, rcvbuf=1<<20;
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		PLOG(ERROR)<<"socket fails";
        return -1;
    }
#ifdef _MSC_VER
	u_long enable = 1;
	ioctlsocket(fd, FIONBIO, &enable);
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char *)&sndbuf, sizeof(int)) != 0 ) {
        PLOG(ERROR)<<"setsockopt fails";
        return -3;
    }
   	if ( setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, sizeof(int)) != 0 ) {
        PLOG(ERROR)<<"setsockopt2 fails";
        return -3;
    }
#else
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		return -2;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int)) < 0 ) {
        PLOG(ERROR)<<"setsockopt fails";
        return -3;
    }
   	if ( setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int)) < 0 ) {
        PLOG(ERROR)<<"setsockopt2 fails";
        return -3;
    }
#endif
    printf("BUFS: %i %i\n",sndbuf,rcvbuf);
    /*memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
    addr.sin_port = htons(portno);
    addr.sin_addr.s_addr = INADDR_ANY;*/
	if (::bind(fd, (sockaddr*)&addr, len) != 0) {
        PLOG(ERROR)<<"bind fails";
        return -4;
    }
	return fd;
}

void Datagram::Close (int sock) { // remove from fd_set
#ifdef _MSC_VER
	if (closesocket(sock)!=0)
#else
	if (::close(sock)!=0)
#endif
		PLOG(ERROR)<<"on closing a socket";
}


std::string sock2str (struct sockaddr_in addr) {
	char ipch[32];
#ifdef _MSC_VER
	//Vista only: InetNtop(AF_INET,&(addr.sin_addr),ipch,32);
	// IPv4 only:
	struct in_addr inaddr;
	memcpy(&inaddr, &(addr.sin_addr), sizeof(inaddr));
	strncpy(ipch, inet_ntoa(inaddr),32);
#else
	inet_ntop(AF_INET,&(addr.sin_addr),ipch,32);
#endif
	sprintf(ipch+strlen(ipch),":%i",ntohs(addr.sin_port));
	return std::string(ipch);
}

/*
std::string Datagram::to_string () const { // TODO: pretty-print P2TP
	std::string addrs = sock2str(addr);
	char hex[MAXDGRAMSZ*2];
	for(int i=offset; i<length; i++)
		sprintf(hex+i*2,"%02x",buf[i]);
	std::string hexs(hex+offset*2,(length-offset)*2);
	return addrs + '\t' + hexs;
}*/

}
