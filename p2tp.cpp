/*
 *  p2tp.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include <stdlib.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
//#include <glog/logging.h>
#include "p2tp.h"
#include "datagram.h"

using namespace std;
using namespace p2tp;

p2tp::tint Channel::last_tick = 0;
int Channel::MAX_REORDERING = 4;
p2tp::tint Channel::TIMEOUT = TINT_SEC*60;
std::vector<Channel*> Channel::channels(1);
int Channel::sockets[8] = {0,0,0,0,0,0,0,0};
int Channel::socket_count = 0;
Address Channel::tracker;
tbqueue Channel::send_queue;
#include "ext/simple_selector.cpp"
PeerSelector* Channel::peer_selector = new SimpleSelector();

Channel::Channel	(FileTransfer* transfer, int socket, Address peer_addr) :
	transfer_(transfer), peer_(peer_addr), peer_channel_id_(0), pex_out_(0),
    socket_(socket==-1?sockets[0]:socket), // FIXME
    own_id_mentioned_(false), next_send_time_(0), last_send_time_(0),
    last_recv_time_(0), rtt_avg_(TINT_SEC), dev_avg_(0), dip_avg_(TINT_SEC)
{
    if (peer_==Address())
        peer_ = tracker;
	this->id = channels.size();
	channels.push_back(this);
    cc_ = new PingPongController(this);
    RequeueSend(NOW);
}


Channel::~Channel () {
	channels[id] = NULL;
    delete cc_;
}


void     p2tp::SetTracker(const Address& tracker) {
    Channel::tracker = tracker;
}


int Channel::DecodeID(int scrambled) {
	return scrambled ^ (int)Datagram::epoch;
}
int Channel::EncodeID(int unscrambled) {
	return unscrambled ^ (int)Datagram::epoch;
}


int     p2tp::Listen (Address addr) {
    int sock = Datagram::Bind(addr);
    if (sock!=INVALID_SOCKET)
        Channel::sockets[Channel::socket_count++] = sock;
    return sock;
}


void    p2tp::Shutdown (int sock_des) {
    for(int i=0; i<Channel::socket_count; i++)
        if (sock_des==-1 || Channel::sockets[i]==sock_des) {
            Datagram::Close(Channel::sockets[i]);
            Channel::sockets[i] = Channel::sockets[--Channel::socket_count];
        }
}


void    p2tp::Loop (tint till) {
    Channel::Loop(till);
}


int      p2tp::Open (const char* filename, const Sha1Hash& hash) {
    FileTransfer* ft = new FileTransfer(filename, hash);
    int fdes = ft->file().file_descriptor();
    if (fdes>0) {
        
        /*if (FileTransfer::files.size()<fdes)  // FIXME duplication
            FileTransfer::files.resize(fdes);
        FileTransfer::files[fdes] = ft;*/
        
        // initiate tracker connections
        if (Channel::tracker!=Address())
            new Channel(ft);
        
        return fdes;
    } else {
        delete ft;
        return -1;
    }
}


void	p2tp::Close (int fd) {
    // FIXME delete all channels
    if (fd>FileTransfer::files.size() && FileTransfer::files[fd])
        delete FileTransfer::files[fd];
}


void    p2tp::AddPeer (Address address, const Sha1Hash& root) {
    Channel::peer_selector->AddPeer(address,root);
}


uint64_t  p2tp::Size (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->file().size();
    else
        return 0;
}


bool  p2tp::IsComplete (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->file().is_complete();
    else
        return 0;
}


uint64_t  p2tp::Complete (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->file().complete();
    else
        return 0;
}


uint64_t  p2tp::SeqComplete (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->file().seq_complete();
    else
        return 0;
}


const Sha1Hash& p2tp::RootMerkleHash (int file) {
    FileTransfer* trans = FileTransfer::file(file);
    if (!trans)
        return Sha1Hash::ZERO;
    return trans->file().root_hash();
}


/**	<h2> P2TP handshake </h2>
 Basic rules:
 <ul>
 <li>	to send a datagram, a channel must be created
 (channels are cheap and easily recycled)
 <li>	a datagram must contain either the receiving
 channel id (scrambled) or the root hash
 <li>	initially, the control structure (p2tp_channel)
 is mostly zeroed; intialization happens as
 conversation progresses
 </ul>
 <b>Note:</b>
 */
