/*
 *  swift.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include <stdlib.h>
#include <fcntl.h>
#ifndef _WIN32
#include <sys/select.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <string.h>

//#include <glog/logging.h>
#include "swift.h"

using namespace std;
using namespace swift;


swift::tint now_t::now = Channel::Time();
tint Channel::start = now_t::now;
tint Channel::epoch = now_t::now/360000000LL*360000000LL; // make logs mergeable
uint64_t Channel::dgrams_up=0, Channel::dgrams_down=0,
         Channel::bytes_up=0, Channel::bytes_down=0;
sckrwecb_t Channel::sock_open[] = {};
int Channel::sock_count = 0;
swift::tint Channel::last_tick = 0;
int Channel::MAX_REORDERING = 4;
bool Channel::SELF_CONN_OK = false;
swift::tint Channel::TIMEOUT = TINT_SEC*60;
std::vector<Channel*> Channel::channels(1);
Address Channel::tracker;
//tbheap Channel::send_queue;
FILE* Channel::debug_file = NULL;
#include "ext/simple_selector.cpp"
PeerSelector* Channel::peer_selector = new SimpleSelector();

Channel::Channel    (FileTransfer* transfer, int socket, Address peer_addr) :
    transfer_(transfer), peer_(peer_addr), peer_channel_id_(0), pex_out_(0),
    socket_(socket==INVALID_SOCKET?default_socket():socket), // FIXME
    data_out_cap_(bin64_t::ALL), last_data_out_time_(0), last_data_in_time_(0),
    own_id_mentioned_(false), next_send_time_(0), last_send_time_(0),
    last_recv_time_(0), rtt_avg_(TINT_SEC), dev_avg_(0), dip_avg_(TINT_SEC),
    data_in_dbl_(bin64_t::NONE), hint_out_size_(0),
    cwnd_(1), send_interval_(TINT_SEC), send_control_(PING_PONG_CONTROL),
    sent_since_recv_(0), ack_rcvd_recent_(0), ack_not_rcvd_recent_(0),
    last_loss_time_(0), owd_min_bin_(0), owd_min_bin_start_(NOW), 
    owd_cur_bin_(0), dgrams_sent_(0), dgrams_rcvd_(0), 
    data_in_(TINT_NEVER,bin64_t::NONE)
{
    if (peer_==Address())
        peer_ = tracker;
    this->id_ = channels.size();
    channels.push_back(this);
    transfer_->hs_in_.push_back(id_);
    for(int i=0; i<4; i++) {
        owd_min_bins_[i] = TINT_NEVER;
        owd_current_[i] = TINT_NEVER;
    }
    evtimer_assign(&evsend,evbase,&Channel::SendCallback,this);
    evtimer_add(&evsend,tint2tv(next_send_time_));
    dprintf("%s #%u init %s\n",tintstr(),id_,peer_.str());
}


Channel::~Channel () {
    channels[id_] = NULL;
    evtimer_del(&evsend);
}

tint Channel::Time () {
    //HiResTimeOfDay* tod = HiResTimeOfDay::Instance();
    //tint ret = tod->getTimeUSec();
    //DLOG(INFO)<<"now is "<<ret;
    return now_t::now = usec_time();
}

SOCKET Channel::Bind (Address address, sckrwecb_t callbacks) {
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
    sock_open[sock_count++] = callbacks;
    return fd;
}

int Channel::SendTo (SOCKET sock, Address addr, struct evbuffer *evb) {
    int length = evbuffer_get_length(evb);
    int r = sendto(sock,(const char *)evbuffer_pullup(evb, length),length,0,
                   (struct sockaddr*)&(addr.addr),sizeof(struct sockaddr_in));
    if (r<0)
        perror("can't send");
    // TODO: evbuffer_drain?
    dgrams_up++;
    bytes_up+=length;
    Time();
    return r;
}

int Channel::RecvFrom (SOCKET sock, Address& addr, struct evbuffer *evb) {
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct evbuffer_iovec vec;
    if (evbuffer_reserve_space(evb, MAXDGRAMSZ*2, &vec, 1) < 0) {
	print_error("error on evbuffer_reserve_space");
	return 0;
    }
    int length = recvfrom (sock, (char *)vec.iov_base, MAXDGRAMSZ*2, 0,
			   (struct sockaddr*)&(addr.addr), &addrlen);
    if (length<0) {
        length = 0;
        print_error("error on recv");
    }
    vec.iov_len = length;
    if (evbuffer_commit_space(evb, &vec, 1) < 0)  {
        length = 0;
        print_error("error on evbuffer_commit_space");
    }
    dgrams_down++;
    bytes_down+=length;
    Time();
    return length;
}

void Channel::Close (SOCKET sock) {
    for(int i=0; i<sock_count; i++)
        if (sock_open[i].sock==sock)
            sock_open[i] = sock_open[--sock_count];
    if (!close_socket(sock))
        print_error("on closing a socket");
}

void Channel::Shutdown () {
    while (sock_count--)
        Close(sock_open[sock_count].sock);
}
    
void     swift::SetTracker(const Address& tracker) {
    Channel::tracker = tracker;
}


int Channel::DecodeID(int scrambled) {
    return scrambled ^ (int)start;
}
int Channel::EncodeID(int unscrambled) {
    return unscrambled ^ (int)start;
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

uint32_t Address::LOCALHOST = INADDR_LOOPBACK;

const char* swift::tintstr (tint time) {
    if (time==0)
        time = now_t::now;
    static char ret_str[4][32]; // wow
    static int i;
    i = (i+1) & 3;
    if (time==TINT_NEVER)
        return "NEVER";
    time -= Channel::epoch;
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

std::string swift::sock2str (struct sockaddr_in addr) {
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

int     swift::Listen (Address addr) {
    sckrwecb_t cb;
    cb.may_read = &Channel::RecvDatagram;
    cb.sock = Channel::Bind(addr,cb);
    // swift UDP receive
    event_assign(&Channel::evrecv, Channel::evbase, cb.sock, EV_READ,
		 Channel::ReceiveCallback, NULL);
    event_add(&Channel::evrecv, NULL);
    return cb.sock;
}


void    swift::Shutdown (int sock_des) {
    Channel::Shutdown();
}


//void    swift::Loop (tint till) {
//    Channel::Loop(till);
//}



int      swift::Open (const char* filename, const Sha1Hash& hash) {
    FileTransfer* ft = new FileTransfer(filename, hash);
    if (ft && ft->file().file_descriptor()) {

        /*if (FileTransfer::files.size()<fdes)  // FIXME duplication
            FileTransfer::files.resize(fdes);
        FileTransfer::files[fdes] = ft;*/

        // initiate tracker connections
        if (Channel::tracker!=Address())
            new Channel(ft);

        return ft->file().file_descriptor();
    } else {
        if (ft)
            delete ft;
        return -1;
    }
}


void    swift::Close (int fd) {
    if (fd<FileTransfer::files.size() && FileTransfer::files[fd])
        delete FileTransfer::files[fd];
}


void    swift::AddPeer (Address address, const Sha1Hash& root) {
    Channel::peer_selector->AddPeer(address,root);
}


uint64_t  swift::Size (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->file().size();
    else
        return 0;
}


bool  swift::IsComplete (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->file().is_complete();
    else
        return 0;
}


uint64_t  swift::Complete (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->file().complete();
    else
        return 0;
}


uint64_t  swift::SeqComplete (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->file().seq_complete();
    else
        return 0;
}


const Sha1Hash& swift::RootMerkleHash (int file) {
    FileTransfer* trans = FileTransfer::file(file);
    if (!trans)
        return Sha1Hash::ZERO;
    return trans->file().root_hash();
}


int swift::evbuffer_add_8(struct evbuffer *evb, uint8_t b) {
    return evbuffer_add(evb, &b, 1);
}

int swift::evbuffer_add_16be(struct evbuffer *evb, uint16_t w) {
    uint16_t wbe = htons(w);
    return evbuffer_add(evb, &wbe, 2);
}

int swift::evbuffer_add_32be(struct evbuffer *evb, uint32_t i) {
    uint32_t ibe = htonl(i);
    return evbuffer_add(evb, &ibe, 4);
}

int swift::evbuffer_add_64be(struct evbuffer *evb, uint64_t l) {
    uint32_t lbe[2];
    lbe[0] = htonl((uint32_t)(l>>32));
    lbe[1] = htonl((uint32_t)(l&0xffffffff));
    return evbuffer_add(evb, lbe, 8);
}

int swift::evbuffer_add_hash(struct evbuffer *evb, const Sha1Hash& hash)  {
    return evbuffer_add(evb, hash.bits, Sha1Hash::SIZE);
}

uint8_t swift::evbuffer_remove_8(struct evbuffer *evb) {
    uint8_t b;
    if (evbuffer_remove(evb, &b, 1) < 1)
	return 0;
    return b;
}

uint16_t swift::evbuffer_remove_16be(struct evbuffer *evb) {
    uint16_t wbe;
    if (evbuffer_remove(evb, &wbe, 2) < 2)
	return 0;
    return ntohs(wbe);
}

uint32_t swift::evbuffer_remove_32be(struct evbuffer *evb) {
    uint32_t ibe;
    if (evbuffer_remove(evb, &ibe, 4) < 4)
	return 0;
    return ntohl(ibe);
}

uint64_t swift::evbuffer_remove_64be(struct evbuffer *evb) {
    uint32_t lbe[2];
    if (evbuffer_remove(evb, lbe, 8) < 8)
	return 0;
    uint64_t l = ntohl(lbe[0]);
    l<<=32;
    l |= ntohl(lbe[1]);
    return l;
}

Sha1Hash swift::evbuffer_remove_hash(struct evbuffer* evb)  {
    char bits[Sha1Hash::SIZE];
    if (evbuffer_remove(evb, bits, Sha1Hash::SIZE) < Sha1Hash::SIZE)
	return Sha1Hash::ZERO;
    return Sha1Hash(false, bits);
}

/**    <h2> swift handshake </h2>
 Basic rules:
 <ul>
 <li>    to send a datagram, a channel must be created
 (channels are cheap and easily recycled)
 <li>    a datagram must contain either the receiving
 channel id (scrambled) or the root hash
 </ul>
 <b>Note:</b>
 */
