/*
 *  datagram.h
 *  nice IPv4 UDP wrappers
 *
 *  Created by Victor Grishchenko, Arno Bakker on 3/9/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef DATAGRAM_H
#define DATAGRAM_H

#include <sys/stat.h>
#include <string.h>
#include "hashtree.h"
#include "compat.h"


namespace swift {

#define MAXDGRAMSZ 2800
#ifndef _WIN32
#define INVALID_SOCKET -1
#endif


/** IPv4 address, just a nice wrapping around struct sockaddr_in. */
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
    void set_ipv4 (const char* ipv4_str) ;
    //{    inet_aton(ipv4_str,&(addr.sin_addr));    }
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
        set_ipv4((uint32_t)INADDR_ANY);
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
    const char* str () const {
        static char rs[4][32];
        static int i;
        i = (i+1) & 3;
        sprintf(rs[i],"%i.%i.%i.%i:%i",ipv4()>>24,(ipv4()>>16)&0xff,
                (ipv4()>>8)&0xff,ipv4()&0xff,port());
        return rs[i];
    }
    bool operator != (const Address& b) const { return !(*this==b); }
};


typedef void (*sockcb_t) (SOCKET);
struct sckrwecb_t {
    sckrwecb_t (SOCKET s=0, sockcb_t mr=NULL, sockcb_t mw=NULL, sockcb_t oe=NULL) :
        sock(s), may_read(mr), may_write(mw), on_error(oe) {}
    SOCKET sock;
    sockcb_t   may_read;
    sockcb_t   may_write;
    sockcb_t   on_error;
};


/** UDP datagram class, a nice wrapping around sendto/recvfrom/select. 
    Reading/writing from/to a datagram is done in a FIFO (deque) fashion:
    written data is appended to the tail (push) while read data is
    taken from the "head" of the buffer. */
class Datagram {

    Address addr;
    SOCKET sock;
    int offset, length;
    uint8_t    buf[MAXDGRAMSZ*2];

#define DGRAM_MAX_SOCK_OPEN 128
    static int sock_count;
    static sckrwecb_t sock_open[DGRAM_MAX_SOCK_OPEN];
    
public:

    /** bind to the address */
    static SOCKET Bind(Address address, sckrwecb_t callbacks=sckrwecb_t());

    /** close the port */
    static void Close(SOCKET sock);

    /** the current time */
    static tint Time();

    /** wait till one of the sockets has some io to do; usec is the timeout */
    static SOCKET Wait (tint usec);
    
    static bool Listen3rdPartySocket (sckrwecb_t cb) ;
    
    static void Shutdown ();
    
    static SOCKET default_socket() 
        { return sock_count ? sock_open[0].sock : INVALID_SOCKET; }

    static tint now, epoch, start;
    static uint64_t dgrams_up, dgrams_down, bytes_up, bytes_down;

    /** This constructor is normally used to SEND something to the address. */
    Datagram (SOCKET socket, const Address addr_) : addr(addr_), offset(0),
        length(0), sock(socket) {}
    /** This constructor is normally used to RECEIVE something at the socket. */
    Datagram (SOCKET socket) : offset(0), length(0), sock(socket) {
    }

    /** space remaining */
    int space () const { return MAXDGRAMSZ-length; }
    /** size of the data (not counting UDP etc headers) */
    int size() const { return length-offset; }
    std::string str() const { return std::string((char*)buf+offset,size()); }
    const uint8_t* operator * () const { return buf+offset; }
    const Address& address () const { return addr; }
    /** Append some data at the back */
    int Push (const uint8_t* data, int l) { // scatter-gather one day
        int toc = l<space() ? l : space();
        memcpy(buf+length,data,toc);
        length += toc;
        return toc;
    }
    /** Read something from the front of the datagram */
    int Pull (uint8_t** data, int l) {
        int toc = l<size() ? l : size();
        //memcpy(data,buf+offset,toc);
        *data = buf+offset;
        offset += toc;
        return toc;
    }

    int Send ();
    int Recv ();

    void Clear() { offset=length=0; }

    void    PushString (std::string str) {
        Push((uint8_t*)str.c_str(),str.size());
    }
    void    Push8 (uint8_t b) {
        buf[length++] = b;
    }
    void    Push16 (uint16_t w) {
        *(uint16_t*)(buf+length) = htons(w);
        length+=2;
    }
    void    Push32 (uint32_t i) {
        *(uint32_t*)(buf+length) = htonl(i);
        length+=4;
    }
    void    Push64 (uint64_t l) {
        *(uint32_t*)(buf+length) = htonl((uint32_t)(l>>32));
        *(uint32_t*)(buf+length+4) = htonl((uint32_t)(l&0xffffffff));
        length+=8;
    }
    void    PushHash (const Sha1Hash& hash) {
        Push((uint8_t*)hash.bits, Sha1Hash::SIZE);
    }

    uint8_t    Pull8() {
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
    //std::string    to_string () const ;

};

const char* tintstr(tint t=0);
std::string sock2str (struct sockaddr_in addr);

}

#endif
