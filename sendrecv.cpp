/*
 *  datasendrecv.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include <algorithm>
#include <glog/logging.h>
#include "p2tp.h"

#include "ext/dummy_controller.cpp"

using namespace std;
using namespace p2tp;

void	Channel::AddPeakHashes (Datagram& dgram) {
	for(int i=0; i<file().peak_count(); i++) {
		dgram.Push8(P2TP_HASH);
		dgram.Push32((uint32_t)file().peak(i));
		dgram.PushHash(file().peak_hash(i));
        DLOG(INFO)<<"#"<<id<<" +pHASH"<<file().peak(i);
	}
}


void	Channel::AddUncleHashes (Datagram& dgram, bin64_t pos) {
    bin64_t peak = file().peak_for(pos);
    while (pos!=peak && ack_in.get(pos.parent())==bins::EMPTY) {
        bin64_t uncle = pos.sibling();
		dgram.Push8(P2TP_HASH);
		dgram.Push32((uint32_t)uncle);
		dgram.PushHash( file().hash(uncle) );
        DLOG(INFO)<<"#"<<id<<" +uHASH"<<uncle;
        pos = pos.parent();
    }
}


bin64_t		Channel::DequeueHint () { // TODO: resilience
	while (!hint_in.empty()) {
		bin64_t hint = hint_in.front();
		hint_in.pop_front();
		if (ack_in.get(hint)==bins::FILLED)
			continue;
		if ( file().ack_out().get(hint)==bins::EMPTY )
			continue;
		if (!hint.is_base()) {
			bin64_t l=hint.left(), r=hint.right();
			//if (rand()&1)
			//	swap(l,r);
			hint_in.push_front(r);
			hint_in.push_front(l);
			continue;
		}
		return hint;
	}
	return bin64_t::NONE;
}


void	Channel::CleanStaleHints () {
	while ( !hint_out.empty() && file().ack_out().get(hint_out.front().bin)==bins::FILLED ) 
		hint_out.pop_front();  // FIXME must normally clear fulfilled entries
	tint timed_out = Datagram::now - cc->rtt_avg()*8;
	while ( !hint_out.empty() && hint_out.front().time < timed_out ) {
        file().picker()->Snubbed(hint_out.front().bin);
		hint_out.pop_front();
	}
}


void	Channel::AddHandshake (Datagram& dgram) {
	dgram.Push8(P2TP_HANDSHAKE);
	dgram.Push32(EncodeID(id));
	if (!peer_channel_id) { // initiating
		dgram.Push8(P2TP_HASH);
		dgram.Push32(bin64_t::ALL32);
		dgram.PushHash(file().root_hash());
	}
    AddAck(dgram);
    //DLOG(INFO)<<"#"<<id<<" sending a handshake to "<<this->id_string();
}


tint	Channel::Send () {
    Datagram dgram(socket_,peer);
    dgram.Push32(peer_channel_id);
    if ( is_established() ) {
        AddAck(dgram);
        AddHint(dgram);
        if (cc->free_cwnd() && Datagram::now>=cc->next_send_time()) {
            bin64_t data = AddData(dgram);
            cc->OnDataSent(data);
        }
    } else {
        AddHandshake(dgram);
    }
    DLOG(INFO)<<"#"<<id<<" sending "<<dgram.size()<<" bytes";
	PCHECK( dgram.Send() != -1 )<<"error sending";
	last_send_time = Datagram::now;
    return cc->next_send_time();
}


void	Channel::AddHint (Datagram& dgram) {
	CleanStaleHints();
    uint64_t outstanding = 0;
    for(tbqueue::iterator i=hint_out.begin(); i!=hint_out.end(); i++)
        outstanding += i->bin.width();
	uint64_t kbps = TINT_SEC * cc->peer_cwnd() / cc->rtt_avg();
    if (outstanding>kbps) // have enough
        return;
    uint8_t layer = 0;
    while( (1<<layer) < kbps ) layer++;
    bin64_t hint = file().picker()->Pick(ack_in,layer);
    if (hint==bin64_t::NONE)
        return;
    dgram.Push8(P2TP_HINT);
    dgram.Push32(hint);
    hint_out.push_back(tintbin(Datagram::now,hint));
    DLOG(INFO)<<"#"<<id<<" +HINT"<<hint;
}


bin64_t		Channel::AddData (Datagram& dgram) {
	if (!file().size()) // know nothing
		return bin64_t::NONE;
	bin64_t tosend = DequeueHint();
    if (tosend==bin64_t::NONE) {
        //LOG(WARNING)<<this->id_string()<<" no idea what to send";
		cc->OnDataSent(bin64_t::NONE);
		return bin64_t::NONE;
	}
	if (ack_in.empty() && file().size())
		AddPeakHashes(dgram);
	AddUncleHashes(dgram,tosend);
	uint8_t buf[1024];
	size_t r = pread(file().file_descriptor(),buf,1024,tosend.base_offset()<<10); // TODO: ??? corrupted data, retries
	if (r<0) {
		PLOG(ERROR)<<"error on reading";
		return 0;
	}
	assert(dgram.space()>=r+4+1);
	dgram.Push8(P2TP_DATA);
	dgram.Push32(tosend);
	dgram.Push(buf,r);
    DLOG(INFO)<<"#"<<id<<" +DATA"<<tosend;
	return tosend;
}


void	Channel::AddAck (Datagram& dgram) {
	if (data_in_.time) {
		dgram.Push8(P2TP_ACK_TS);
		dgram.Push32(data_in_.bin);
		dgram.Push64(data_in_.time);
        data_in_.time = 0;
        DLOG(INFO)<<"#"<<id<<" +!ACK"<<data_in_.bin;
	}
    bin64_t h=file().data_in(ack_out_);
    int count=0;
    while (h!=bin64_t::NONE && count++<4) {
        dgram.Push8(P2TP_ACK);
        dgram.Push32(h);
        DLOG(INFO)<<"#"<<id<<" +ACK"<<h;
        h=file().data_in(++ack_out_);
    }
}


void	Channel::Recv (Datagram& dgram) {
	while (dgram.size()) {
		uint8_t type = dgram.Pull8();
		switch (type) {
            case P2TP_HANDSHAKE: OnHandshake(dgram); break;
			case P2TP_DATA:		OnData(dgram); break;
			case P2TP_ACK_TS:	OnAckTs(dgram); break;
			case P2TP_ACK:		OnAck(dgram); break;
			case P2TP_HASH:		OnHash(dgram); break;
			case P2TP_HINT:		OnHint(dgram); break;
            case P2TP_PEX_ADD:  OnPex(dgram); break;
			default:
				//LOG(ERROR) << this->id_string() << " malformed datagram";
				return;
		}
	}
}


void	Channel::OnHash (Datagram& dgram) {
	bin64_t pos = dgram.Pull32();
	Sha1Hash hash = dgram.PullHash();
	file().OfferHash(pos,hash);
    DLOG(INFO)<<"#"<<id<<" .HASH"<<(int)pos;
}


void Channel::OnData (Datagram& dgram) {
	bin64_t pos = dgram.Pull32();
    DLOG(INFO)<<"#"<<id<<" .DATA"<<pos;
    file().OfferData(pos, *dgram, dgram.size());
	cc->OnDataRecvd(pos);
	CleanStaleHints();
}


void	Channel::OnAck (Datagram& dgram) {
	// note: no bound checking
	bin64_t pos = dgram.Pull32();
    DLOG(INFO)<<"#"<<id<<" .ACK"<<pos;
	ack_in.set(pos);
}


void	Channel::OnAckTs (Datagram& dgram) {
	bin64_t pos = dgram.Pull32();
    tint ts = dgram.Pull64();
    DLOG(INFO)<<"#"<<id<<" ,ACK"<<pos;
    //dprintf("%lli #%i +ack %lli +ts %lli",Datagram::now,id,pos,ts);
	ack_in.set(pos);
	cc->OnAckRcvd(tintbin(ts,pos));
}


void	Channel::OnHint (Datagram& dgram) {
	bin64_t hint = dgram.Pull32();
	hint_in.push_back(hint);
}


void Channel::OnHandshake (Datagram& dgram) {
    peer_channel_id = dgram.Pull32();
    // FUTURE: channel forking
}


void Channel::OnPex (Datagram& dgram) {
    uint32_t addr = dgram.Pull32();
    uint16_t port = dgram.Pull16();
    if (peer_selector)
        peer_selector->AddPeer(Datagram::Address(addr,port),file().root_hash());
}


void	Channel::Recv (int socket) {
	Datagram data(socket);
	data.Recv();
	if (data.size()<4) 
		RETLOG("datagram shorter than 4 bytes");
	uint32_t mych = data.Pull32();
	Sha1Hash hash;
	Channel* channel;
	if (!mych) { // handshake initiated
		if (data.size()<1+4+1+4+Sha1Hash::SIZE) 
			RETLOG ("incorrect size initial handshake packet");
		uint8_t hashid = data.Pull8();
		if (hashid!=P2TP_HASH) 
			RETLOG ("no hash in the initial handshake");
		bin pos = data.Pull32();
		if (pos!=bin64_t::ALL32) 
			RETLOG ("that is not the root hash");
		hash = data.PullHash();
		FileTransfer* file = FileTransfer::Find(hash);
		if (!file) 
			RETLOG ("hash unknown, no such file");
		channel = new Channel(file, socket, data.address());
	} else {
		mych = DecodeID(mych);
		if (mych>=channels.size()) 
			RETLOG ("invalid channel id");
		channel = channels[mych];
		if (!channel) 
			RETLOG ("channel is closed");
		if (channel->peer != data.address()) 
			RETLOG ("invalid peer address");
		channel->Recv(data);
	}
	channel->Send();
}


bool tblater (const tintbin& a, const tintbin& b) {
    return a.time > b.time;
}


void    Channel::Loop (tint time) {  
	
	tint untiltime = Datagram::Time()+time;
    tbqueue send_queue;
    for(int i=0; i<channels.size(); i++)
        if (channels[i])
            send_queue.push_back(tintbin(Datagram::now,i));
	
    while ( Datagram::now <= untiltime ) {
		
        tintbin next_send = send_queue.front();
        pop_heap(send_queue.begin(), send_queue.end(), tblater);
        send_queue.pop_back();
        tint wake_on = min(next_send.time,untiltime);
		tint towait = min(wake_on-Datagram::now,TINT_SEC); // towait<0?
        
		int rd = Datagram::Wait(socket_count,sockets,towait);
		if (rd!=-1)
			Recv(rd);
        
        int chid = (int)(next_send.bin);
        Channel* sender = channels[chid];
        if (sender) {
            tint next_time = sender->Send();
            if (next_time!=TINT_NEVER) {
                send_queue.push_back(tintbin(next_time,chid));
                push_heap(send_queue.begin(),send_queue.end(),tblater);
            } else {
                delete sender;
                channels[chid] = NULL;
            }
        }
		
    }
	
}
