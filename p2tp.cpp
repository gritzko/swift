/*
 *  p2tp.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
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
std::vector<File*> File::files(4);
int* Channel::sockets_ = (int*)malloc(40);
int Channel::sock_count_ = 0;


Channel::Channel	(int fd_, int socket, struct sockaddr_in peer_,
					 uint32_t peer_channel_, uint64_t supports_) :
	fd(fd_), peer(peer_), peer_channel_id(peer_channel_), ack_out(0),
	peer_status_(File::EMPTY), socket_(socket)
{
	this->id = channels.size();
	channels.push_back(this);
	DLOG(INFO)<<"new channel "<<id<<" "<<*this;
}


Channel::~Channel () {
	channels[id] = NULL;
}


File::File (int _fd) : fd(_fd), status_(DONE), hashes(_fd)
{
	bin::vec peaks = bin::peaks(hashes.data_size());
	history.insert(history.end(),peaks.begin(),peaks.end());
	for(bin::vec::iterator i=peaks.begin(); i!=peaks.end(); i++)
		ack_out.set(*i);
}

File::File (Sha1Hash hash, int _fd) : hashes(hash), fd(_fd), status_(EMPTY) {
	// TODO resubmit data
}

File::~File() {
	if (fd>0) ::close(fd);
}


bool	File::OfferHash (bin pos, const Sha1Hash& hash) {
	HashTree::hashres_t res = hashes.offer(pos,hash);
	if (res==HashTree::PEAK_ACCEPT) { // file size is finally known
		ftruncate(fd, size());
		LOG(INFO)<<fd<<" file size is set to "<<size();
		history.push_back(0);
		status_ = IN_PROGRESS;
	}
	return res==HashTree::PEAK_ACCEPT || res==HashTree::ACCEPT;
}


File*	File::find (const Sha1Hash& hash) {
	for(vector<File*>::iterator i=files.begin(); i!=files.end(); i++)
		if (*i && (*i)->hashes.root==hash)
			return *i;
	return NULL;
}

int Channel::DecodeID(int scrambled) {
	return scrambled;
}
int Channel::EncodeID(int unscrambled) {
	return unscrambled;
}


std::ostream& p2tp::operator << (std::ostream& os, const Channel& ch) {
	return os<<'{'<<ch.fd<<'}'<<sock2str(ch.peer)<<":"<<ch.id<<'>'<<ch.peer_channel_id;
}


void	Channel::Recv (int socket) {
	Datagram data(socket);
	data.Recv();
	//LOG(INFO)<<" RECV "<<data.to_string();
	int id = 0;
	if (data.size()<4) 
		RETLOG("datagram shorter than 4 bytes");
	uint32_t mych = data.Pull32();
	uint8_t type;
	uint32_t peerch;
	Sha1Hash hash;
	Channel* channel;
	if (!mych) { // handshake initiated
		if (data.size()!=1+4+1+4+Sha1Hash::SIZE) 
			RETLOG ("incorrect size initial handshake packet");
		type = data.Pull8();
		if (type)  // handshake msg id is 0
			RETLOG ("it is not actually a handshake");
		peerch = data.Pull32();
		if (!peerch) 
			RETLOG ("peer channel is zero");
		uint8_t hashid = data.Pull8();
		if (hashid!=P2TP_HASH) 
			RETLOG ("no hash in the initial handshake");
		bin pos = data.Pull32();
		if (pos!=bin::ALL) 
			RETLOG ("that is not the root hash");
		hash = data.PullHash();
		File* file = File::find(hash);
		if (!file) 
			RETLOG ("hash unknown, no such file");
		channel = new Channel(file->fd, socket, data.address(), peerch);
	} else {
		mych = DecodeID(mych);
		if (mych>=channels.size()) 
			RETLOG ("invalid channel id");
		channel = channels[mych];
		id = channel->id;
		if (channel->peer.sin_addr.s_addr != data.address().sin_addr.s_addr) 
			RETLOG ("invalid peer address");
		if (channel->peer.sin_port!=data.address().sin_port) 
			RETLOG ("invalid peer port");
		if (!channel->peer_channel_id) { // handshake response
			if (data.size()<5) 
				RETLOG ("insufficient return handshake length");
			type = data.Pull8();
			if (type) 
				RETLOG ("it is not a handshake, after all");
			channel->peer_channel_id = data.Pull32();
			LOG(INFO)<<"out channel is open: "<<*channel;
		} else if (channel->cc_.avg_rtt()==0) {
			LOG(INFO)<<"in channel is open: "<<*channel;
		}
        if (channel->cc_.avg_rtt()==0)
            channel->cc_.RttSample(Datagram::now - channel->last_send_time + 1);
		channel->Recv(data);
	}
	channel->Send();
}


void		Channel::Tick () {
	// choking/unchoking
	// keepalives
	// ack timeout
	// if unchoked: don't bother
	// whether to unchoke
	// reevaluate reciprocity
	// otherwise, send update (if needed)
	// otherwise, send a keepalive
	CleanStaleHintIn();
	CleanStaleHintOut();
	if (last_send_time && Datagram::now-last_send_time>=Channel::TIMEOUT/2)
		Send();
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


void    Channel::Loop (tint time) {  
	
	tint untiltime = Datagram::Time()+time;
	
    while ( Datagram::now <= untiltime ) {
		
		tint towait = min(untiltime,Datagram::now+TINT_1SEC) - Datagram::now;
		int rd = Datagram::Wait(sock_count_,sockets_,towait);
		if (rd!=-1)
			Recv(rd);

		/*if (Datagram::now-last_tick>TINT_1SEC) {
			for(int i=0; i<channels.size(); i++)
				if (channels[i])
					channels[i]->Tick();
			last_tick = Datagram::now;
		}*/
		
    }
	
}

int p2tp::Open (const char* filename) {
	int fd = ::open(filename,O_RDONLY);
	if (fd<0)
		return -1;
	if (File::files.size()<fd+1)
		File::files.resize(fd+1);
	File::files[fd] = new File(fd);
	return fd;
}

int p2tp::Open (const Sha1Hash& root_hash, const char* filename) {
	int fd = ::open(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd<0)
		return -1;
	if (File::files.size()<fd+1)
		File::files.resize(fd+1);
	File::files[fd] = new File(root_hash,fd);
	return fd;
}

size_t	p2tp::file_size (int fd) { return File::file(fd)->size(); }

void p2tp::Close (int fid) {
	if (!File::files[fid])
		return;
	delete File::files[fid];
	File::files[fid] = NULL;
}


int p2tp::Connect (int fd, int sock, const struct sockaddr_in& addr, uint32_t peerch) {
	Channel *ch = new Channel(fd,sock,addr,peerch);
	ch->Send();
	return ch->id;
}

void p2tp::Loop (tint time) {
	Channel::Loop(time);
}

int p2tp::Init (int portno) {
	int sock = Datagram::Bind(portno);
	if (sock>0)
		Channel::sockets_[Channel::sock_count_++] = sock;
	return sock;
}

void p2tp::Shutdown (int sock) {
	int i=0;
	while (i<Channel::sock_count_ && Channel::sockets_[i]!=sock) i++;
	if (i==Channel::sock_count_) {
		LOG(ERROR)<<"socket "<<sock<<" is unknown to p2tp";
		return;
	}
	Channel::sockets_[i] = Channel::sockets_[--Channel::sock_count_];
	Datagram::Close(sock);
}


uint32_t p2tp::Width (const tbinvec& v) {
	uint32_t ret = 0;
	for(tbinvec::const_iterator i=v.begin(); i!=v.end(); i++)
		ret += i->pos.width();
	return ret;
}

