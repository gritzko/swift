/*
 *  datagram.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko, Arno Bakker on 3/9/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    typedef int socklen_t;
#else
    #include <arpa/inet.h>
    #include <netdb.h>
#endif

#include "datagram.h"
#include "compat.h"

namespace p2tp {

tint Datagram::now = Datagram::Time();
tint Datagram::start = now;
tint Datagram::epoch = now/360000000LL*360000000LL;
uint32_t Address::LOCALHOST = INADDR_LOOPBACK;
uint64_t Datagram::dgrams_up=0, Datagram::dgrams_down=0,
         Datagram::bytes_up=0, Datagram::bytes_down=0;

const char* tintstr (tint time) {
    if (time==0)
        time = Datagram::now;
    static char ret_str[4][32]; // wow
    static int i;
    i = (i+1) & 3;
    if (time==TINT_NEVER)
        return "NEVER";
    time -= Datagram::epoch;
    assert(time>=0);
    int hours = time/TINT_HOUR;
    time %= TINT_HOUR;
    int mins = time/TINT_MIN;
    time %= TINT_MIN;
    int secs = time/TINT_SEC;
    time %= TINT_SEC;
    int msecs = time/TINT_MSEC;
    time %= TINT_MSEC;
    int usecs = time/TINT_uSEC;
    sprintf(ret_str[i],"%i_%02i_%02i_%03i_%03i",hours,mins,secs,msecs,usecs);
    return ret_str[i];
}

void Address::set_ipv4 (const char* ip_str) {
    struct hostent *h = gethostbyname(ip_str);
    if (h == NULL) {
    	print_error("cannot lookup address");
    	return;
    } else {
        addr.sin_addr.s_addr = *(u_long *) h->h_addr_list[0];
    }
}


Address::Address(const char* ip_port) {
    clear();
    if (strlen(ip_port)>=1024)
        return;
    char ipp[1024];
    strncpy(ipp,ip_port,1024);
    char* semi = strchr(ipp,':');
    if (semi) {
        *semi = 0;
        set_ipv4(ipp);
        set_port(semi+1);
    } else {
        if (strchr(ipp, '.')) {
            set_ipv4(ipp);
            set_port((uint16_t)0);
        } else {
            set_ipv4(INADDR_LOOPBACK);
            set_port(ipp);
        }
    }
}


int Datagram::Send () {
	int r = sendto(sock,(const char *)buf+offset,length-offset,0,
				   (struct sockaddr*)&(addr.addr),sizeof(struct sockaddr_in));
    if (r<0)
        perror("can't send");
	//offset=0;
	//length=0;
    dgrams_up++;
    bytes_up+=size();
	Time();
	return r;
}

int Datagram::Recv () {
	socklen_t addrlen = sizeof(struct sockaddr_in);
	offset = 0;
	length = recvfrom (sock, (char *)buf, MAXDGRAMSZ, 0,
					   (struct sockaddr*)&(addr), &addrlen);
	if (length<0) {
        length = 0;
        print_error("error on recv");
    }
    dgrams_down++;
    bytes_down+=length;
	Time();
	return length;
}


SOCKET Datagram::Wait (int sockcnt, SOCKET* sockets, tint usec) {
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
    Time();
	if (sel>0) {
		for (int i=0; i<=sockcnt; i++)
			if (FD_ISSET(sockets[i],&bases))
				return sockets[i];
	} else if (sel<0) {
		print_error("select fails");
    }
    return INVALID_SOCKET;
}

tint Datagram::Time () {
	//HiResTimeOfDay* tod = HiResTimeOfDay::Instance();
	//tint ret = tod->getTimeUSec();
	//DLOG(INFO)<<"now is "<<ret;
	return now = usec_time();
}

SOCKET Datagram::Bind (Address addr_) {
    struct sockaddr_in addr = addr_;
	SOCKET fd;
	int len = sizeof(struct sockaddr_in), sndbuf=1<<20, rcvbuf=1<<20;
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		print_error("socket() fails");
        return INVALID_SOCKET;
    }
#ifdef _WIN32
	u_long enable = 1;
	ioctlsocket(fd, FIONBIO, &enable);
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char *)&sndbuf, sizeof(int)) != 0 ) {
        print_error("setsockopt fails");
        return INVALID_SOCKET;
    }
   	if ( setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, sizeof(int)) != 0 ) {
        print_error("setsockopt2 fails");
        return INVALID_SOCKET;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int));
#else
    int enable=1;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		return INVALID_SOCKET;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(int)) < 0 ) {
        print_error("setsockopt fails");
        return INVALID_SOCKET;
    }
   	if ( setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int)) < 0 ) {
        print_error("setsockopt2 fails");
        return INVALID_SOCKET;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
#endif
    dprintf("socket buffers: %i send %i recv\n",sndbuf,rcvbuf);
	if (::bind(fd, (sockaddr*)&addr, len) != 0) {
        print_error("bind fails");
        return INVALID_SOCKET;
    }
	return fd;
}

void Datagram::Close (int sock) { // remove from fd_set
#ifdef _WIN32
	if (closesocket(sock)!=0)
#else
	if (::close(sock)!=0)
#endif
		print_error("on closing a socket");
}


std::string sock2str (struct sockaddr_in addr) {
	char ipch[32];
#ifdef _WIN32
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
