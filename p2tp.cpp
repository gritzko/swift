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
#include <glog/logging.h>
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


Channel::Channel	(FileTransfer* file, int socket, struct sockaddr_in peer_addr) :
	file_(file), peer(peer_addr), peer_channel_id(0),
	socket_(socket) // FIXME
{
	this->id = channels.size();
	channels.push_back(this);
}


Channel::~Channel () {
	channels[id] = NULL;
}




int Channel::DecodeID(int scrambled) {
	return scrambled;
}
int Channel::EncodeID(int unscrambled) {
	return unscrambled;
}


int     p2tp::Listen (Datagram::Address addr) {
    int sock = Datagram::Bind(addr);
    if (sock!=INVALID_SOCKET)
        Channel::sockets[Channel::socket_count++] = sock;
    return sock;
}


void    p2tp::Shutdown (int sock_des) {
    for(int i=0; i<Channel::socket_count; i++)
        if (Channel::sockets[i]==sock_des)
            Channel::sockets[i] = Channel::sockets[--Channel::socket_count];
    Datagram::Close(sock_des);
}


void    p2tp::Loop (tint till) {
    Channel::Loop(till);
}


void	p2tp::Close (int fd) {
    // FIXME delete all channels
    if (fd>FileTransfer::files.size() && FileTransfer::files[fd])
        delete FileTransfer::files[fd];
}


void    p2tp::AddPeer (Datagram::Address address, const Sha1Hash& root) {
    Channel::peer_selector->AddPeer(address,root);
}


size_t  p2tp::Size (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->size();
    else
        return 0;
}


size_t  p2tp::Complete (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->complete();
    else
        return 0;
}


size_t  p2tp::SeqComplete (int fdes) {
    if (FileTransfer::files.size()>fdes && FileTransfer::files[fdes])
        return FileTransfer::files[fdes]->seq_complete();
    else
        return 0;
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
