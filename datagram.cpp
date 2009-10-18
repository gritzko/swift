/*
 *  datagram.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/9/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include <arpa/inet.h>
#include <glog/logging.h>
#include "datagram.h"

namespace p2tp {

tint Datagram::now = Datagram::Time();

int Datagram::Send () {
	int r = sendto(sock,buf+offset,length-offset,0,
				   (struct sockaddr*)&(addr),sizeof(struct sockaddr_in));
	offset=0;
	length=0;
	now = Time();
	return r;
}

int Datagram::Recv () {
	socklen_t addrlen = sizeof(struct sockaddr_in);
	offset = 0;
	length = recvfrom (sock, buf, MAXDGRAMSZ, 0, 
					   (struct sockaddr*)&(addr), &addrlen);
	if (length<0)
		PLOG(ERROR)<<"on recv";
	now = Time();
	return length;
}


int Datagram::Wait (int sockcnt, int* sockets, tint usec) {
	LOG(INFO)<<"waiting for "<<sockcnt;
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
		PLOG(ERROR)<<"select fails";
	return -1;
}

tint Datagram::Time () {
	struct timeval t;
	gettimeofday(&t,NULL);
	tint ret;
	ret = t.tv_sec;
	ret *= 1000000;
	ret += t.tv_usec;
	//DLOG(INFO)<<"now is "<<ret;
	return now=ret;
}

int Datagram::Bind (int portno) {
    struct sockaddr_in addr;
	int fd, len = sizeof(struct sockaddr_in), 
        sndbuf=1<<20, rcvbuf=1<<20;
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		PLOG(ERROR)<<"socket fails";
        return -1;
    }
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		return -2;
	if ( setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int)) < 0 ) {
        PLOG(ERROR)<<"setsockopt fails";
        return -3;
    }
   	if ( setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int)) < 0 ) {
        PLOG(ERROR)<<"setsockopt2 fails";
        return -3;
    }
    printf("BUFS: %i %i\n",sndbuf,rcvbuf);
    memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
    addr.sin_port = htons(portno);
    addr.sin_addr.s_addr = INADDR_ANY;
	if (::bind(fd, (struct sockaddr*)&addr, len) != 0) {
        PLOG(ERROR)<<"bind fails";
        return -4;
    }
	return fd;
}

void Datagram::Close (int sock) { // remove from fd_set
	if (::close(sock)!=0)
		PLOG(ERROR)<<"on closing a socket";
}


std::string sock2str (struct sockaddr_in addr) {
	char ipch[32];
	inet_ntop(AF_INET,&(addr.sin_addr),ipch,32);
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
