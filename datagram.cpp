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

namespace swift {

tint Datagram::now = Datagram::Time();
tint Datagram::start = now;
tint Datagram::epoch = now/360000000LL*360000000LL; // make logs mergeable
uint32_t Address::LOCALHOST = INADDR_LOOPBACK;
uint64_t Datagram::dgrams_up=0, Datagram::dgrams_down=0,
         Datagram::bytes_up=0, Datagram::bytes_down=0;
sckrwecb_t Datagram::sock_open[] = {};
int Datagram::sock_count = 0;

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

    
bool    Datagram::Listen3rdPartySocket (sckrwecb_t cb) {
    int i=0;
    while (i<sock_count && sock_open[i].sock!=cb.sock) i++;
    if (i==sock_count)
        if (i==DGRAM_MAX_SOCK_OPEN)
            return false;
        else
            sock_count++;
    sock_open[i]=cb;
    //if (!cb.may_read && !cb.may_write && !cb.on_error)
    //    sock_open[i] = sock_open[--sock_count];
    return true;
}

    
void Datagram::Shutdown () {
    while (sock_count--)
        Close(sock_open[sock_count].sock);
}
    

int Datagram::Send () {
    int r = sendto(sock,(const char *)buf+offset,length-offset,0,
                   (struct sockaddr*)&(addr.addr),sizeof(struct sockaddr_in));
    if (r<0)
        perror("can't send");
    dgrams_up++;
    bytes_up+=size();
    offset=0;
    length=0;
    Time();
    return r;
}

int Datagram::Recv () {
    socklen_t addrlen = sizeof(struct sockaddr_in);
    offset = 0;
    length = recvfrom (sock, (char *)buf, MAXDGRAMSZ*2, 0,
                       (struct sockaddr*)&(addr.addr), &addrlen);
    if (length<0) {
        length = 0;
        print_error("error on recv");
    }
    dgrams_down++;
    bytes_down+=length;
    Time();
    return length;
}


SOCKET Datagram::Wait (tint usec) {
    struct timeval timeout;
    timeout.tv_sec = usec/TINT_SEC;
    timeout.tv_usec = usec%TINT_SEC;
    int max_sock_fd = 0;
    fd_set rdfd, wrfd, errfd;
    FD_ZERO(&rdfd);
    FD_ZERO(&wrfd);
    FD_ZERO(&errfd);
    for(int i=0; i<sock_count; i++) {
        if (sock_open[i].may_read!=0)
            FD_SET(sock_open[i].sock,&rdfd);
        if (sock_open[i].may_write!=0)
            FD_SET(sock_open[i].sock,&wrfd);
        if (sock_open[i].on_error!=0)
            FD_SET(sock_open[i].sock,&errfd);
        if (sock_open[i].sock>max_sock_fd)
            max_sock_fd = sock_open[i].sock;
    }
    SOCKET sel = select(max_sock_fd+1, &rdfd, &wrfd, &errfd, &timeout);
    Time();
    if (sel>0) {
        for (int i=0; i<=sock_count; i++) {
            sckrwecb_t& sct = sock_open[i];
            if (sct.may_read && FD_ISSET(sct.sock,&rdfd))
                (*(sct.may_read))(sct.sock);
            if (sct.may_write && FD_ISSET(sct.sock,&wrfd))
                (*(sct.may_write))(sct.sock);
            if (sct.on_error && FD_ISSET(sct.sock,&errfd))
                (*(sct.on_error))(sct.sock);
        }
    } else if (sel<0) {
        print_error("select fails");
    }
    return sel;
}

tint Datagram::Time () {
    //HiResTimeOfDay* tod = HiResTimeOfDay::Instance();
    //tint ret = tod->getTimeUSec();
    //DLOG(INFO)<<"now is "<<ret;
    return now = usec_time();
}

SOCKET Datagram::Bind (Address address, sckrwecb_t callbacks) {
    struct sockaddr_in addr = address;
    SOCKET fd;
    int len = sizeof(struct sockaddr_in), sndbuf=1<<20, rcvbuf=1<<20;
    #define dbnd_ensure(x) { if (!(x)) { \
        print_error("binding fails"); close_socket(fd); return INVALID_SOCKET; } }
    dbnd_ensure ( (fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0 );
    dbnd_ensure( make_socket_nonblocking(fd) );  // FIXME may remove this
    int enable = true;
    dbnd_ensure ( setsockopt(fd, SOL_SOCKET, SO_SNDBUF, 
                             (setsockoptptr_t)&sndbuf, sizeof(int)) == 0 );
    dbnd_ensure ( setsockopt(fd, SOL_SOCKET, SO_RCVBUF, 
                             (setsockoptptr_t)&rcvbuf, sizeof(int)) == 0 );
    //setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (setsockoptptr_t)&enable, sizeof(int));
    dbnd_ensure ( ::bind(fd, (sockaddr*)&addr, len) == 0 );
    callbacks.sock = fd;
    Datagram::sock_open[Datagram::sock_count++] = callbacks;
    return fd;
}

void Datagram::Close (SOCKET sock) {
    for(int i=0; i<Datagram::sock_count; i++)
        if (Datagram::sock_open[i].sock==sock)
            Datagram::sock_open[i] = Datagram::sock_open[--Datagram::sock_count];
    if (!close_socket(sock))
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
std::string Datagram::to_string () const { // TODO: pretty-print swift
    std::string addrs = sock2str(addr);
    char hex[MAXDGRAMSZ*2];
    for(int i=offset; i<length; i++)
        sprintf(hex+i*2,"%02x",buf[i]);
    std::string hexs(hex+offset*2,(length-offset)*2);
    return addrs + '\t' + hexs;
}*/

}
