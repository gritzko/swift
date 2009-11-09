/*
 *  datagram.h
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/9/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef DATAGRAM_H
#define DATAGRAM_H

#ifdef _WIN32
    #include "compat/stdint.h"
    #include <winsock2.h>
	#include "compat/unixio.h"
#else
    typedef int SOCKET;
    #include <stdint.h>
    #include <arpa/inet.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
#endif
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include "hashtree.h"
#include "compat/hirestimeofday.h"
#include "compat/util.h"


namespace p2tp {

typedef int64_t tint;
#define TINT_HOUR ((tint)1000000*60*60)
#define TINT_MIN ((tint)1000000*60)
#define TINT_SEC ((tint)1000000)
#define TINT_MSEC ((tint)1000)
#define TINT_uSEC ((tint)1)
#define TINT_NEVER ((tint)0x7fffffffffffffffLL)
#define MAXDGRAMSZ 1400
#ifndef _WIN32
#define INVALID_SOCKET -1
#endif

    
struct Address {
    struct sockaddr_in  addr;
    static uint32_t LOCALHOST;
    void set_port (uint16_t port) {
        addr.sin_port = htons(port);
    }
    void set_port (const char* port_str) {
        int p;
        if (sscanf(port_str,"%i",&p))
            set_port(p);
    }
    void set_ipv4 (uint32_t ipv4) {
        addr.sin_addr.s_addr = htonl(ipv4);
    }
    void set_ipv4 (const char* ipv4_str) {
        inet_aton(ipv4_str,&(addr.sin_addr));
    }
    void clear () {
        memset(&addr,0,sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
    }
    Address() { 
        clear();
    }
    Address(const char* ip, uint16_t port)  {
        clear();
        set_ipv4(ip);
        set_port(port);
    }
    Address(const char* ip_port);
    Address(uint16_t port) {
        clear();
        set_ipv4(LOCALHOST);
        set_port(port);
    }
    Address(uint32_t ipv4addr, uint16_t port) {
        clear();
        set_ipv4(ipv4addr);
        set_port(port);
    }
    Address(const struct sockaddr_in& address) : addr(address) {}
    uint32_t ipv4 () const { return ntohl(addr.sin_addr.s_addr); }
    uint16_t port () const { return ntohs(addr.sin_port); }
    operator sockaddr_in () const {return addr;}
    bool operator == (const Address& b) const { 
        return addr.sin_family==b.addr.sin_family &&
        addr.sin_port==b.addr.sin_port &&
        addr.sin_addr.s_addr==b.addr.sin_addr.s_addr;
    }
    std::string str () const {
        char s[32];
        sprintf(s,"%i.%i.%i.%i:%i",ipv4()>>24,(ipv4()>>16)&0xff,
                (ipv4()>>8)&0xff,ipv4()&0xff,port());
        return std::string(s);
    }
    bool operator != (const Address& b) const { return !(*this==b); }
};
    
    
struct Datagram {

	Address addr;
	SOCKET sock;
	int offset, length;
	uint8_t	buf[MAXDGRAMSZ*2];

	static SOCKET Bind(Address address);
	static void Close(int port);
	static tint Time();
	static SOCKET Wait (int sockcnt, SOCKET* sockets, tint usec=0);
	static tint now, epoch;
    static uint64_t dgrams_up, dgrams_down, bytes_up, bytes_down;

	Datagram (SOCKET socket, const Address addr_) : addr(addr_), offset(0),
		length(0), sock(socket) {}
	Datagram (SOCKET socket) : offset(0), length(0), sock(socket) {
	}

	int space () const { return MAXDGRAMSZ-length; }
	int size() const { return length-offset; }
	std::string str() const { return std::string((char*)buf+offset,size()); }
    const uint8_t* operator * () const { return buf+offset; }

	int Push (const uint8_t* data, int l) { // scatter-gather one day
		int toc = l<space() ? l : space();
		memcpy(buf+length,data,toc);
		length += toc;
		return toc;
	}
	int Pull (uint8_t** data, int l) {
		int toc = l<size() ? l : size();
		//memcpy(data,buf+offset,toc);
		*data = buf+offset;
		offset += toc;
		return toc;
	}

	int Send ();
	int Recv ();
	const Address& address() const { return addr; }
    void Clear() { offset=length=0; }

	void	PushString (std::string str) {
		Push((uint8_t*)str.c_str(),str.size());
	}
	void	Push8 (uint8_t b) {
		buf[length++] = b;
	}
	void	Push16 (uint16_t w) {
		*(uint16_t*)(buf+length) = htons(w);
		length+=2;
	}
	void	Push32 (uint32_t i) {
		*(uint32_t*)(buf+length) = htonl(i);
		length+=4;
	}
	void	Push64 (uint64_t l) {
		*(uint32_t*)(buf+length) = htonl((uint32_t)(l>>32));
		*(uint32_t*)(buf+length+4) = htonl((uint32_t)(l&0xffffffff));
		length+=8;
	}
	void	PushHash (const Sha1Hash& hash) {
		Push((uint8_t*)hash.bits, Sha1Hash::SIZE);
	}

	uint8_t	Pull8() {
		if (size()<1) return 0;
		return buf[offset++];
	}
	uint16_t Pull16() {
		if (size()<2) return 0;
		offset+=2;
		return ntohs(*(uint16_t*)(buf+offset-2));
	}
	uint32_t Pull32() {
		if (size()<4) return 0;
		uint32_t i = ntohl(*(uint32_t*)(buf+offset));
		offset+=4;
		return i;
	}
	uint64_t Pull64() {
		if (size()<8) return 0;
		uint64_t l = ntohl(*(uint32_t*)(buf+offset));
		l<<=32;
		l |= ntohl(*(uint32_t*)(buf+offset+4));
		offset+=8;
		return l;
	}
	Sha1Hash PullHash() {
		if (size()<Sha1Hash::SIZE) return Sha1Hash::ZERO;
		offset += Sha1Hash::SIZE;
		return Sha1Hash(false,(char*)buf+offset-Sha1Hash::SIZE);
	}
	//std::string	to_string () const ;

};

const char* tintstr(tint t=0);
std::string sock2str (struct sockaddr_in addr);
#define dprintf(...) printf(__VA_ARGS__)
#define eprintf(...) fprintf(stderr,__VA_ARGS__)

}

#endif
