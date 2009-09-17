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

using namespace std;
using namespace p2tp;


void	Channel::AddPeakHashes (Datagram& dgram) {
	const std::vector<binhash>& peaks = file().hashes.peak_hashes();
	for(int i=0; i<peaks.size(); i++) {
		dgram.Push8(P2TP_HASH);
		dgram.Push32(peaks[i].first);
		dgram.PushHash(peaks[i].second);
        DLOG(INFO)<<"#"<<id<<" +pHASH"<<peaks[i].first;
	}
}


void	Channel::AddUncleHashes (Datagram& dgram, bin pos) {
	bin root = pos;
	while (root.parent()<=file().hashes.data_mass() && ack_in.clean(root.parent()))
		root = root.parent();
	while (root!=pos) {
		root = root.child(pos);
		bin uncle = root.sibling();
		dgram.Push8(P2TP_HASH);
		dgram.Push32(uncle);
		dgram.PushHash( file().hashes[uncle] );
        DLOG(INFO)<<"#"<<id<<" +uHASH"<<uncle;
	}
}


bin		Channel::SenderPiecePick () { // TODO: resilience
	while (!hint_in.empty()) {
		bin hint = hint_in.front().pos;
		hint_in.pop_front();
		if (ack_in.contains(hint))
			continue;
		if (hint.layer()) {
			bin l=hint.left(), r=hint.right();
			//if (false)//rand()&1)
			//	swap(l,r);
			hint_in.push_front(tintbin(Datagram::now,r));
			hint_in.push_front(tintbin(Datagram::now,l));
			continue;
		}
		if ( !file().ack_out.contains(hint) )
			continue;
		return hint;
	}
	return 0;
}



Channel::state_t	Channel::state () const {
	if (!peer_channel_id)
		return HS_REQ_OUT;
	if (cc_.avg_rtt()==0)
		return HS_RES_OUT;
	return HS_DONE;
}


void	Channel::CleanStaleDataOut (bin ack_pos) {

	if (ack_pos)
		for(int i=0; i<data_out.size() && i<MAX_REORDERING*2; i++)
			if (data_out[i].pos && ack_pos.contains(data_out[i].pos)) {
				cc_.RttSample(Datagram::now-data_out[i].time);
				cc_.OnCongestionEvent(CongestionControl::ACK_EV);
				data_out[i].pos = 0;
			}
	while (!data_out.empty() && data_out[0].pos==0)
		data_out.pop_front();

	tint timed_out = Datagram::now - cc_.safe_avg_rtt();
	while (!data_out.empty() && data_out.front().time < timed_out) {
		DLOG(INFO)<<*this<<" loss: "<<data_out.front().pos;
		// reordering is a loss, collision is a loss
		cc_.OnCongestionEvent(CongestionControl::LOSS_EV);
		data_out.pop_front();
	}
	
}


void	Channel::CleanStaleHintOut () {
	while ( !hint_out.empty() && file().ack_out.contains(hint_out.front().pos) ) 
		hint_out.pop_front();
	tint timed_out = Datagram::now - cc_.safe_avg_rtt()*4; //FIXME: timeout
	while ( !hint_out.empty() && hint_out.front().time < timed_out ) {
		file().hint_out -= hint_out.front().pos;
		hint_out.pop_front(); // TODO: ignore count
	}
}

void	Channel::CleanStaleHintIn () {
	// do I need it?
}


void	Channel::SendHandshake () {
	Datagram dgram(socket_,peer);
	dgram.Push32(peer_channel_id);
	dgram.Push8(P2TP_HANDSHAKE);
	dgram.Push32(EncodeID(id));
	if (!peer_channel_id) { // initiating
		dgram.Push8(P2TP_HASH);
		dgram.Push32(bin::ALL);
		dgram.PushHash(file().hashes.root);
		AddAck(dgram);
	} else { // responding
		AddAck(dgram);
	}
    DLOG(INFO)<<"#"<<id<<" sending a handshake to "<<*this;
	PCHECK( dgram.Send() != -1 )<<"error sending";
	last_send_time = Datagram::now;
}


void		Channel::SendData () {
	CleanStaleDataOut(0);
	int round = 0;
    Datagram dgram(socket_,peer);
    dgram.Push32(peer_channel_id);
    AddAck(dgram);
    AddHint(dgram);
	while (cc_.cwnd()>data_out.size()) {
		AddData(dgram); // always the last: might be tail block
		if (dgram.size()==4 && Datagram::now-last_send_time<TIMEOUT/2) 
			break; // nothing to send
        DLOG(INFO)<<"#"<<id<<" sending "<<dgram.size()<<" bytes";
		PCHECK( dgram.Send() != -1 )<<"error sending";
		last_send_time = Datagram::now;
		round++;
        dgram.Clear();
        dgram.Push32(peer_channel_id);
	}
	DLOG(INFO)<<"#"<<id<<" sent "<<round<<" rounds";
}


void	Channel::Send () {
	if (state()==HS_DONE)
		SendData();
	else
		SendHandshake();
}


bin		Channel::ReceiverPiecePick (int limit) {
	bins diff(ack_in);
	diff -= file().ack_out;
	diff -= file().hint_out;
	if (diff.empty()) {
		//uninteresting = true;
		return 0;
	}
	bin need = *(diff.begin());
	while (need.width()>std::max(1,limit))
		need = need.left();
	return need;
}


void	Channel::AddHint (Datagram& dgram) {
	CleanStaleHintOut();
	int onesec = TINT_1SEC/cc_.data_in_rate();
	if (Width(hint_out)<onesec) {
		bin hint = ReceiverPiecePick(onesec);
		if (hint) {
			dgram.Push8(P2TP_HINT);
			dgram.Push32(hint);
			hint_out.push_back(tintbin(Datagram::now,hint));
			file().hint_out |= hint;  // FIXME: incapsulate File data
            DLOG(INFO)<<"#"<<id<<" +HINT"<<hint;
		}
	}
}


bin		Channel::AddData (Datagram& dgram) {
	if (!file().history.size()) // know nothing
		return 0;
	bin tosend = hash_out ? hash_out : SenderPiecePick();
	hash_out = 0;
    if (tosend==0) {
        LOG(WARNING)<<*this<<" no idea what to send";
		cc_.OnCongestionEvent(CongestionControl::LOSS_EV);
		return 0;
	}
	if (peer_status()==File::EMPTY && file().history.size()) //FIXME
		AddPeakHashes(dgram);
	AddUncleHashes(dgram,tosend);
	uint8_t buf[1024];
	size_t r = pread(fd,buf,1024,tosend.offset()<<10); // TODO: ??? corrupted data, retries
	if (r<0) {
		PLOG(ERROR)<<"error on reading";
		return 0;
	}
	if (dgram.space()<r+4+1) {
		hash_out = tosend;
		return -tosend; // FIXME
	}
	dgram.Push8(P2TP_DATA);
	dgram.Push32(tosend);
	dgram.Push(buf,r);
	data_out.push_back(tintbin(Datagram::Time(),tosend));
    DLOG(INFO)<<"#"<<id<<" +DATA"<<tosend;
	return tosend;
}


void	Channel::AddAck (Datagram& dgram) {
	int ackspace = min(4,dgram.space()/5);
	if (data_in_) {
		dgram.Push8(P2TP_ACK);
		dgram.Push32(data_in_);
        DLOG(INFO)<<"#"<<id<<" +!ACK"<<data_in_;
		ackspace--;
	}
	while (ack_out<file().history.size() && ackspace) {
		bin h=file().history[ack_out++];
		if (!file().ack_out.contains(h.parent()) && h!=data_in_) {
			dgram.Push8(P2TP_ACK);
			dgram.Push32(h);
            DLOG(INFO)<<"#"<<id<<" +ACK"<<h;
			ackspace--;
		}
	}
	data_in_ = 0;
}


void	Channel::Recv (Datagram& dgram) {
	while (dgram.size()) {
		uint8_t type = dgram.Pull8();
		switch (type) {
			case P2TP_DATA:		OnData(dgram); break;
			case P2TP_ACK:		OnAck(dgram); break;
			case P2TP_HASH:		OnHash(dgram); break;
			case P2TP_HINT:		OnHint(dgram); break;
			default:
				LOG(ERROR) << *this << " malformed datagram";
				return;
		}
	}
}


void	Channel::OnHash (Datagram& dgram) {
	bin pos = dgram.Pull32();
	Sha1Hash hash = dgram.PullHash();
	if (file().OfferHash(pos,hash))
        DLOG(INFO)<<"#"<<id<<" .HASH"<<(int)pos;
}


void Channel::OnData (Datagram& dgram) {
	bin pos = dgram.Pull32();
	uint8_t* data;
	size_t length = dgram.Pull(&data,1024);
    DLOG(INFO)<<"#"<<id<<" .DATA"<<pos;
	if (pos.layer()) 
		RETLOG("non-base layer DATA pos");
	if (file().ack_out.contains(pos)) 
		RETLOG("duplicate transmission: "<<pos);
	if (file().status()==File::EMPTY)
		RETLOG("DATA for an empty file");
	if (pos.offset()>=file().packet_size())
		RETLOG("DATA pos out of bounds");
	Sha1Hash hash(data,length);
	if (file().OfferHash(pos, hash)) {
		//memcpy(file->data+offset*KILO,
		//channel->datagram->data+channel->datagram->offset,KILO);
		pwrite(fd,data,length,pos.offset()*1024); // TODO; if (last) ftruncate
		if (pos==file().hashes.data_mass()) {
			int lendiff = 1024-length;
			ftruncate(fd, file().size()-lendiff);
		}
		data_in_ = pos;
		file().ack_out |= pos;
		file().history.push_back(file().ack_out.get(pos));
		if (file().history.size()==file().packet_size()+1) // FIXME: encapsulate
			file().status_ = File::DONE;
		cc_.OnCongestionEvent(CongestionControl::DATA_EV);
		//DLOG(INFO)<<*this<<" DATA< "<<pos;
		CleanStaleHintOut();
	} else
		LOG(ERROR)<<"data hash is not accepted "<<pos<<" len "<<length;
}


void	Channel::OnAck (Datagram& dgram) {
	
	bin pos = dgram.Pull32();
    DLOG(INFO)<<"#"<<id<<" .ACK"<<pos;
	if (file().hashes.data_mass() && pos>file().hashes.data_mass()) {
		LOG(WARNING) << "out-of-bounds ACK";
		return;
	}
	ack_in |= pos;
	
	CleanStaleDataOut(pos);
	
	if (peer_status_==File::EMPTY) {
		peer_status_ = File::IN_PROGRESS;
	} else if (peer_status_==File::IN_PROGRESS) {
		// FIXME: FINISHED  ack_in_.filled(file().size())
	}
}


void	Channel::OnHint (Datagram& dgram) {
	bin hint = dgram.Pull32();
	hint_in.push_back(tintbin(Datagram::now,hint));
}


