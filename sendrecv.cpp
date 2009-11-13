/*
 *  datasendrecv.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "p2tp.h"
#include "compat/util.h"


using namespace p2tp;
using namespace std; // FIXME remove

/*
 TODO  25 Oct 18:55
 - move hint_out_, piece picking to piece picker (needed e.g. for the case of channel drop)
 - ANY_LAYER
 - range: ALL
 - randomized testing of advanced ops (new testcase)
 - PeerCwnd()
 - bins hint_out_, tbqueue hint_out_ts_
 
 */

void	Channel::AddPeakHashes (Datagram& dgram) {
	for(int i=0; i<file().peak_count(); i++) {
        bin64_t peak = file().peak(i);
		dgram.Push8(P2TP_HASH);
		dgram.Push32((uint32_t)peak);
		dgram.PushHash(file().peak_hash(i));
        //DLOG(INFO)<<"#"<<id<<" +pHASH"<<file().peak(i);
        dprintf("%s #%i +phash (%i,%lli)\n",tintstr(),id,peak.layer(),peak.offset());
	}
}


void	Channel::AddUncleHashes (Datagram& dgram, bin64_t pos) {
    bin64_t peak = file().peak_for(pos);
    while (pos!=peak && ((NOW&7)==7 || !data_out_cap_.within(pos.parent())) &&
            ack_in_.get(pos.parent())==bins::EMPTY) {
        bin64_t uncle = pos.sibling();
		dgram.Push8(P2TP_HASH);
		dgram.Push32((uint32_t)uncle);
		dgram.PushHash( file().hash(uncle) );
        //DLOG(INFO)<<"#"<<id<<" +uHASH"<<uncle;
        dprintf("%s #%i +hash (%i,%lli)\n",tintstr(),id,uncle.layer(),uncle.offset());
        pos = pos.parent();
    }
}


bin64_t		Channel::DequeueHint () { // TODO: resilience
    bin64_t send = bin64_t::NONE;
    while (!hint_in_.empty() && send==bin64_t::NONE) {
        bin64_t hint = hint_in_.front().bin;
        tint time = hint_in_.front().time;
        hint_in_.pop_front();
        if (time < NOW-2*TINT_SEC ) //NOW-8*rtt_avg_)
            continue;
        send = file().ack_out().find_filtered(ack_in_,hint,bins::FILLED);
        send = send.left_foot(); // single packet
        dprintf("%s #%i dequeued %lli\n",tintstr(),id,send.base_offset());
        if (send!=bin64_t::NONE)
            while (send!=hint) {
                hint = hint.towards(send);
                hint_in_.push_front(hint.sibling());
            }
    }
    return send;
}


/*void	Channel::CleanStaleHints () {
	while ( !hint_out.empty() && file().ack_out().get(hint_out.front().bin)==bins::FILLED ) 
		hint_out.pop_front();  // FIXME must normally clear fulfilled entries
	tint timed_out = NOW - cc_->RoundTripTime()*8;
	while ( !hint_out.empty() && hint_out.front().time < timed_out ) {
        file().picker()->Snubbed(hint_out.front().bin);
		hint_out.pop_front();
	}
}*/


void	Channel::AddHandshake (Datagram& dgram) {
	if (!peer_channel_id_) { // initiating
		dgram.Push8(P2TP_HASH);
		dgram.Push32(bin64_t::ALL32);
		dgram.PushHash(file().root_hash());
        dprintf("%s #%i +hash ALL %s\n",
                tintstr(),id,file().root_hash().hex().c_str());
	}
	dgram.Push8(P2TP_HANDSHAKE);
	dgram.Push32(EncodeID(id));
    dprintf("%s #%i +hs\n",tintstr(),id);
    ack_out_.clear();
    AddAck(dgram);
}


void    Channel::ClearStaleDataOut() {
    int oldsize = data_out_.size();
    while ( data_out_.size() && data_out_.front().time < 
           NOW - rtt_avg_ - dev_avg_*4 )
        data_out_.pop_front();
    if (data_out_.size()!=oldsize) {
        cc_->OnAckRcvd(bin64_t::NONE);
        data_out_cap_ = bin64_t::ALL;
    }
    while (data_out_.size() && ack_in_.get(data_out_.front().bin)==bins::FILLED)
        data_out_.pop_front();
}


void	Channel::Send () {
    Datagram dgram(socket_,peer());
    dgram.Push32(peer_channel_id_);
    bin64_t data = bin64_t::NONE;
    if ( is_established() ) {
        // FIXME: seeder check
        AddAck(dgram);
        if (!file().is_complete())
            AddHint(dgram);
        AddPex(dgram);
        ClearStaleDataOut();
        if (cc_->MaySendData()) 
            data = AddData(dgram);
        else
            dprintf("%s #%i no cwnd\n",tintstr(),id);
    } else {
        AddHandshake(dgram);
        AddAck(dgram);
    }
    dprintf("%s #%i sent %ib %s\n",tintstr(),id,dgram.size(),peer().str().c_str());
    if (dgram.size()==4) // only the channel id; bare keep-alive
        data = bin64_t::ALL;
    cc_->OnDataSent(data);
    if (dgram.Send()==-1)
        print_error("can't send datagram");
}


void	Channel::AddHint (Datagram& dgram) {

    int peer_cwnd = (int)(rtt_avg_ / dip_avg_);
    if (!peer_cwnd)
        peer_cwnd = 1;
    int peer_pps = TINT_SEC / dip_avg_;
    if (!peer_pps)
        peer_pps = 1;
    dprintf("%s #%i hint_out_ %lli+%lli mark (%i,%lli) peer_cwnd %lli/%lli=%f\n",
            tintstr(),id,hint_out_,hint_out_am_,(int)hint_out_mark_.bin.layer(),
            hint_out_mark_.bin.offset(),
            rtt_avg_,dip_avg_,((float)rtt_avg_/dip_avg_));
    
    if ( hint_out_mark_.time < NOW - TINT_SEC*2 ) { //NOW-rtt_avg_*8-dev_avg_) {
        hint_out_mark_.bin=bin64_t::NONE;
        hint_out_ = hint_out_am_;
        hint_out_am_ = 0;
    }
    
    if ( peer_pps > hint_out_+hint_out_am_ ) {  //4*peer_cwnd
        
        int diff = peer_pps - hint_out_ - hint_out_am_;  // 4*peer_cwnd
        if (diff>4 && diff>2*peer_cwnd)
            diff >>= 1;
        bin64_t hint = transfer().picker().Pick(ack_in_,diff,NOW+TINT_SEC*3/2); //rtt_avg_*8+TINT_MSEC*10
        
        if (hint!=bin64_t::NONE) {
            dgram.Push8(P2TP_HINT);
            dgram.Push32(hint);
            dprintf("%s #%i +hint (%i,%lli)\n",tintstr(),id,hint.layer(),hint.offset());
            if (hint_out_mark_.bin==bin64_t::NONE)
                hint_out_mark_ = hint;
            hint_out_am_ += hint.width();
            //hint_out_ += hint.width();
        }
        
    }
}


bin64_t		Channel::AddData (Datagram& dgram) {
	if (!file().size()) // know nothing
		return bin64_t::NONE;
	bin64_t tosend = DequeueHint();
    if (tosend==bin64_t::NONE) {
        dprintf("%s #%i out of hints #sendctrl\n",tintstr(),id);
        return bin64_t::NONE;
    }
    if (ack_in_.is_empty() && file().size())
        AddPeakHashes(dgram);
    AddUncleHashes(dgram,tosend);
    uint8_t buf[1024];
    size_t r = pread(file().file_descriptor(),buf,1024,tosend.base_offset()<<10); 
    // TODO: ??? corrupted data, retries
    if (r<0) {
        print_error("error on reading");
        return bin64_t::NONE;
    }
    assert(dgram.space()>=r+4+1);
    dgram.Push8(P2TP_DATA);
    dgram.Push32(tosend);
    dgram.Push(buf,r);
    dprintf("%s #%i +data (%lli)\n",tintstr(),id,tosend.base_offset());
    data_out_.push_back(tosend);
    data_out_cap_ = tosend;
    // FIXME BUG this makes data_out_ all stale  ack_in_.set(tosend);
	return tosend;
}


void	Channel::AddTs (Datagram& dgram) {
    dgram.Push8(P2TP_TS);
    dgram.Push64(data_in_.time);
    dprintf("%s #%i +ts %lli\n",tintstr(),id,data_in_.time);
}


void	Channel::AddAck (Datagram& dgram) {
	if (data_in_.bin!=bin64_t::NONE) {
        AddTs(dgram);
        bin64_t pos = data_in_.bin;
		dgram.Push8(P2TP_ACK);
		dgram.Push32(pos);
		//dgram.Push64(data_in_.time);
        ack_out_.set(pos);
        dprintf("%s #%i +ack (%i,%lli) %s\n",tintstr(),id,
                pos.layer(),pos.offset(),tintstr(data_in_.time));
        data_in_ = tintbin(0,bin64_t::NONE);
	}
    for(int count=0; count<4; count++) {
        bin64_t ack = file().ack_out().find_filtered(ack_out_, bin64_t::ALL, bins::FILLED);
        if (ack==bin64_t::NONE)
            break;
        ack = file().ack_out().cover(ack);
        ack_out_.set(ack);
        dgram.Push8(P2TP_ACK);
        dgram.Push32(ack);
        dprintf("%s #%i +ack (%i,%lli)\n",tintstr(),id,ack.layer(),ack.offset());
    }
}


void	Channel::Recv (Datagram& dgram) {
    if (last_send_time_ && rtt_avg_==TINT_SEC && dev_avg_==0) {
        rtt_avg_ = NOW - last_send_time_;
        dev_avg_ = rtt_avg_;
        dip_avg_ = rtt_avg_;
        transfer().hs_in_.push_back(id);
        dprintf("%s #%i rtt init %lli\n",tintstr(),id,rtt_avg_);
    }
    bin64_t data = dgram.size() ? bin64_t::NONE : bin64_t::ALL;
	while (dgram.size()) {
		uint8_t type = dgram.Pull8();
		switch (type) {
            case P2TP_HANDSHAKE: OnHandshake(dgram); break;
			case P2TP_DATA:		data=OnData(dgram); break;
			case P2TP_TS:       OnTs(dgram); break;
			case P2TP_ACK:		OnAck(dgram); break;
			case P2TP_HASH:		OnHash(dgram); break;
			case P2TP_HINT:		OnHint(dgram); break;
            case P2TP_PEX_ADD:  OnPex(dgram); break;
			default:
				//LOG(ERROR) << this->id_string() << " malformed datagram";
				return;
		}
	}
    cc_->OnDataRecvd(data);
    last_recv_time_ = NOW;
    if (data!=bin64_t::ALL && next_send_time_>NOW+TINT_MSEC)
        Send();
}


void	Channel::OnHash (Datagram& dgram) {
	bin64_t pos = dgram.Pull32();
	Sha1Hash hash = dgram.PullHash();
	file().OfferHash(pos,hash);
    //DLOG(INFO)<<"#"<<id<<" .HASH"<<(int)pos;
    dprintf("%s #%i -hash (%i,%lli)\n",tintstr(),id,pos.layer(),pos.offset());
}


bin64_t Channel::OnData (Datagram& dgram) {
	bin64_t pos = dgram.Pull32();
    uint8_t *data;
    int length = dgram.Pull(&data,1024);
    bool ok = file().OfferData(pos, (char*)data, length) ;
    dprintf("%s #%i %cdata (%lli)\n",tintstr(),id,ok?'-':'!',pos.offset());
    if (!ok) 
        return bin64_t::NONE;
    data_in_ = tintbin(NOW,pos);
    transfer().picker().Received(pos); // FIXME ugly
    if (last_data_time_) {
        tint dip = NOW - last_data_time_;
        dip_avg_ = ( dip_avg_*3 + dip ) >> 2;
    }
    last_data_time_ = NOW;
    if (pos.within(hint_out_mark_.bin)) {
        hint_out_mark_.bin = bin64_t::NONE;
        hint_out_ = hint_out_am_;
        hint_out_am_ = 0;
    }
    if (hint_out_)
        hint_out_--;
    else if (hint_out_am_) // probably, the marking HINT was lost or whatever
        hint_out_am_--;
    return pos;
}


void	Channel::OnAck (Datagram& dgram) {
	bin64_t ackd_pos = dgram.Pull32();
    if (file().size() && ackd_pos.base_offset()>=file().packet_size()) {
        eprintf("invalid ack: (%i,%lli)\n",ackd_pos.layer(),ackd_pos.offset());
        return;
    }
    dprintf("%s #%i -ack (%i,%lli)\n",tintstr(),id,ackd_pos.layer(),ackd_pos.offset());
    for (int i=0; i<8 && i<data_out_.size(); i++) 
        if (data_out_[i].bin.within(ackd_pos)) {
            tint rtt = NOW-data_out_[i].time;
            rtt_avg_ = (rtt_avg_*3 + rtt) >> 2;
            dev_avg_ = ( dev_avg_*3 + abs(rtt-rtt_avg_) ) >> 2;
            dprintf("%s #%i rtt %lli dev %lli\n",
                    tintstr(),id,rtt_avg_,dev_avg_);
            cc_->OnAckRcvd(data_out_[i].bin); // may be invoked twice FIXME FIXME FIXME 
        }
	ack_in_.set(ackd_pos);
    while (data_out_.size() && ack_in_.get(data_out_.front().bin)==bins::FILLED)
        data_out_.pop_front();
}


/*void	Channel::OnAckTs (Datagram& dgram) {  // FIXME:   OnTs
	bin64_t pos = dgram.Pull32();
    tint ts = dgram.Pull64();
    // TODO sanity check
    dprintf("%s #%i -ackts (%i,%lli) %s\n",
            tintstr(),id,pos.layer(),pos.offset(),tintstr(ts));
	ack_in_.set(pos);
	cc_->OnAckRcvd(pos,ts);
}*/

void Channel::OnTs (Datagram& dgram) {
    peer_send_time_ = dgram.Pull64();
    dprintf("%s #%i -ts %lli\n",tintstr(),id,peer_send_time_);
}


void	Channel::OnHint (Datagram& dgram) {
	bin64_t hint = dgram.Pull32();
	hint_in_.push_back(hint);
    //ack_in_.set(hint,bins::EMPTY);
    //RequeueSend(cc_->OnHintRecvd(hint));
    dprintf("%s #%i -hint (%i,%lli)\n",tintstr(),id,hint.layer(),hint.offset());
}


void Channel::OnHandshake (Datagram& dgram) {
    peer_channel_id_ = dgram.Pull32();
    dprintf("%s #%i -hs %i\n",tintstr(),id,peer_channel_id_);
    // FUTURE: channel forking
}


void Channel::OnPex (Datagram& dgram) {
    uint32_t ipv4 = dgram.Pull32();
    uint16_t port = dgram.Pull16();
    Address addr(ipv4,port);
    dprintf("%s #%i -pex %s\n",tintstr(),id,addr.str().c_str());
    transfer().OnPexIn(addr);
}


void    Channel::AddPex (Datagram& dgram) {
    int chid = transfer().RevealChannel(pex_out_);
    if (chid==-1 || chid==id)
        return;
    Address a = channels[chid]->peer();
    dgram.Push8(P2TP_PEX_ADD);
    dgram.Push32(a.ipv4());
    dgram.Push16(a.port());
    dprintf("%s #%i +pex %s\n",tintstr(),id,a.str().c_str());
}


void	Channel::RecvDatagram (int socket) {
	Datagram data(socket);
	data.Recv();
	if (data.size()<4) 
		RETLOG("datagram shorter than 4 bytes");
	uint32_t mych = data.Pull32();
	Sha1Hash hash;
	Channel* channel = NULL;
	if (!mych) { // handshake initiated
		if (data.size()<1+4+1+4+Sha1Hash::SIZE) 
			RETLOG ("incorrect size initial handshake packet");
		uint8_t hashid = data.Pull8();
		if (hashid!=P2TP_HASH) 
			RETLOG ("no hash in the initial handshake");
		bin64_t pos = data.Pull32();
		if (pos!=bin64_t::ALL32) 
			RETLOG ("that is not the root hash");
		hash = data.PullHash();
		FileTransfer* file = FileTransfer::Find(hash);
		if (!file) 
			RETLOG ("hash unknown, no such file");
        dprintf("%s #0 -hash ALL %s\n",tintstr(),hash.hex().c_str());
        for(binqueue::iterator i=file->hs_in_.begin(); i!=file->hs_in_.end(); i++)
            if (channels[*i] && channels[*i]->peer_==data.addr) 
                RETLOG("have a channel already");
		channel = new Channel(file, socket, data.address());
	} else {
		mych = DecodeID(mych);
		if (mych>=channels.size()) {
            eprintf("invalid channel #%i\n",mych);
            return;
        }
		channel = channels[mych];
		if (!channel) 
			RETLOG ("channel is closed");
		if (channel->peer() != data.address()) 
			RETLOG ("invalid peer address");
        channel->own_id_mentioned_ = true;
	}
    dprintf("recvd %i bytes for %i\n",data.size(),channel->id);
    channel->Recv(data);
}


bool tblater (const tintbin& a, const tintbin& b) {
    return a.time > b.time;
}


void    Channel::RequeueSend (tint next_time) {
    if (next_time==next_send_time_)
        return;
    next_send_time_ = next_time;
    send_queue.push_back
        (tintbin(next_time==TINT_NEVER?NOW+TINT_MIN:next_time,id));
    push_heap(send_queue.begin(),send_queue.end(),tblater);
    dprintf("%s requeue #%i for %s\n",tintstr(),id,tintstr(next_time));
}


void    Channel::Loop (tint howlong) {  
	
    tint limit = Datagram::Time() + howlong;
    
    do {

        tint send_time(TINT_NEVER);
        Channel* sender(NULL);
        while (!send_queue.empty()) {
            send_time = send_queue.front().time;
            sender = channel((int)send_queue.front().bin);
            if (sender)
                if ( sender->next_send_time_==send_time ||
                     sender->next_send_time_==TINT_NEVER )
                break;
            sender = NULL; // it was a stale entry
            pop_heap(send_queue.begin(), send_queue.end(), tblater);
            send_queue.pop_back();
        }
        if (send_time>limit)
            send_time = limit;
        if ( sender && sender->next_send_time_ <= NOW ) {
            dprintf("%s #%i sch_send %s\n",tintstr(),sender->id,
                    tintstr(send_time));
            sender->Send();
            sender->last_send_time_ = NOW;
            sender->RequeueSend(sender->cc_->NextSendTime());
            pop_heap(send_queue.begin(), send_queue.end(), tblater);
            send_queue.pop_back();
        } else if ( send_time > NOW ) {
            tint towait = send_time - NOW;
            dprintf("%s waiting %lliusec\n",tintstr(),towait);
            int rd = Datagram::Wait(socket_count,sockets,towait);
            if (rd!=INVALID_SOCKET)
                RecvDatagram(rd);
        } else { //if (sender->next_send_time_==TINT_NEVER) { 
            dprintf("%s #%i closed sendctrl\n",tintstr(),sender->id);
            delete sender;
            pop_heap(send_queue.begin(), send_queue.end(), tblater);
            send_queue.pop_back();
        }
        
    } while (Datagram::Time()<limit);
    	
}

