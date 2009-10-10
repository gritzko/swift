/*
 *  datagram.h
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/9/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#ifndef DATAGRAM_H
#define DATAGRAM_H
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
//#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include "hashtree.h"

namespace p2tp {

typedef int64_t tint;
#define TINT_SEC ((tint)1000000)
#define TINT_MSEC ((tint)1000)
#define TINT_uSEC ((tint)1)
#define TINT_NEVER ((tint)0x7fffffffffffffffLL)
#define MAXDGRAMSZ 1400

struct Datagram {
	struct sockaddr_in addr;
	int sock;
	int offset, length;
	uint8_t	buf[MAXDGRAMSZ];
	
	static int Bind(int port);
	static void Close(int port);
	static tint Time();
	static int Wait (int sockcnt, int* sockets, tint usec=0);
	static tint now;
	
	Datagram (int socket, struct sockaddr_in& addr_) : addr(addr_), offset(0), 
		length(0), sock(socket) {}
	Datagram (int socket) : offset(0), length(0), sock(socket) { 
		memset(&addr,0,sizeof(struct sockaddr_in)); 
	}
	
	int space () const { return MAXDGRAMSZ-length; }
	int size() const { return length-offset; }
	std::string str() const { return std::string((char*)buf+offset,size()); }
	
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
	const struct sockaddr_in& address() const { return addr; }
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
		Push(hash.bits, Sha1Hash::SIZE);
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
	std::string	to_string () const ;
	
};

std::string sock2str (struct sockaddr_in addr);

}

#endif
