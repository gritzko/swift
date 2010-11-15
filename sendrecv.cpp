/*
 *  sendrecv.cpp
 *  most of the swift's state machine
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "swift.h"
#include <algorithm>  // kill it

using namespace swift;
using namespace std;

struct event_base *Channel::evbase;
struct event Channel::evrecv;

/*
 TODO  25 Oct 18:55
 - range: ALL
 - randomized testing of advanced ops (new testcase)
 */

void    Channel::AddPeakHashes (struct evbuffer *evb) {
    for(int i=0; i<file().peak_count(); i++) {
        bin64_t peak = file().peak(i);
        evbuffer_add_8(evb, SWIFT_HASH);
        evbuffer_add_32be(evb, (uint32_t)peak);
        evbuffer_add_hash(evb, file().peak_hash(i));
        dprintf("%s #%u +phash %s\n",tintstr(),id_,peak.str());
    }
}


void    Channel::AddUncleHashes (struct evbuffer *evb, bin64_t pos) {
    bin64_t peak = file().peak_for(pos);
    while (pos!=peak && ((NOW&3)==3 || !data_out_cap_.within(pos.parent())) &&
            ack_in_.get(pos.parent())==binmap_t::EMPTY  ) {
        bin64_t uncle = pos.sibling();
        evbuffer_add_8(evb, SWIFT_HASH);
        evbuffer_add_32be(evb, (uint32_t)uncle);
        evbuffer_add_hash(evb,  file().hash(uncle) );
        dprintf("%s #%u +hash %s\n",tintstr(),id_,uncle.str());
        pos = pos.parent();
    }
}


bin64_t           Channel::ImposeHint () {
    uint64_t twist = peer_channel_id_;  // got no hints, send something randomly
    twist &= file().peak(0); // FIXME may make it semi-seq here
    file().ack_out().twist(twist);
    ack_in_.twist(twist);
    bin64_t my_pick =
        file().ack_out().find_filtered(ack_in_,bin64_t::ALL,binmap_t::FILLED);
    while (my_pick.width()>max(1,(int)cwnd_))
        my_pick = my_pick.left();
    file().ack_out().twist(0);
    ack_in_.twist(0);
    return my_pick.twisted(twist);
}


bin64_t        Channel::DequeueHint () {
    if (hint_in_.empty() && last_recv_time_>NOW-rtt_avg_-TINT_SEC) {
        bin64_t my_pick = ImposeHint(); // FIXME move to the loop
        if (my_pick!=bin64_t::NONE) {
            hint_in_.push_back(my_pick);
            dprintf("%s #%u *hint %s\n",tintstr(),id_,my_pick.str());
        }
    }
    bin64_t send = bin64_t::NONE;
    while (!hint_in_.empty() && send==bin64_t::NONE) {
        bin64_t hint = hint_in_.front().bin;
        tint time = hint_in_.front().time;
        hint_in_.pop_front();
        while (!hint.is_base()) { // FIXME optimize; possible attack
            hint_in_.push_front(tintbin(time,hint.right()));
            hint = hint.left();
        }
        //if (time < NOW-TINT_SEC*3/2 )
        //    continue;  bad idea
        if (ack_in_.get(hint)!=binmap_t::FILLED)
            send = hint;
    }
    uint64_t mass = 0;
    for(int i=0; i<hint_in_.size(); i++)
        mass += hint_in_[i].bin.width();
    dprintf("%s #%u dequeued %s [%lli]\n",tintstr(),id_,send.str(),mass);
    return send;
}


void    Channel::AddHandshake (struct evbuffer *evb) {
    if (!peer_channel_id_) { // initiating
        evbuffer_add_8(evb, SWIFT_HASH);
        evbuffer_add_32be(evb, bin64_t::ALL32);
        evbuffer_add_hash(evb, file().root_hash());
        dprintf("%s #%u +hash ALL %s\n",
                tintstr(),id_,file().root_hash().hex().c_str());
    }
    evbuffer_add_8(evb, SWIFT_HANDSHAKE);
    int encoded = EncodeID(id_);
    evbuffer_add_32be(evb, encoded);
    dprintf("%s #%u +hs %x\n",tintstr(),id_,encoded);
    have_out_.clear();
    AddHave(evb);
}


void    Channel::Send () {
    // Datagram dgram(socket_,peer());
    struct evbuffer *evb = evbuffer_new();
    evbuffer_add_32be(evb, peer_channel_id_);
    bin64_t data = bin64_t::NONE;
    if ( is_established() ) {
        // FIXME: seeder check
        AddHave(evb);
        AddAck(evb);
        if (!file().is_complete())
            AddHint(evb);
        AddPex(evb);
        TimeoutDataOut();
	data = AddData(evb);
    } else {
        AddHandshake(evb);
        AddHave(evb);
        AddAck(evb);
    }
    dprintf("%s #%u sent %ib %s:%x\n",
            tintstr(),id_,evbuffer_get_length(evb),peer().str(),
	    peer_channel_id_);
    if (evbuffer_get_length(evb)==4) {// only the channel id; bare keep-alive
        data = bin64_t::ALL;
    }
    if (Channel::SendTo(socket_,peer(),evb)==-1)
        print_error("can't send datagram");
    last_send_time_ = NOW;
    sent_since_recv_++;
    dgrams_sent_++;
    evbuffer_free(evb);
    Reschedule();
}


void    Channel::AddHint (struct evbuffer *evb) {

    tint plan_for = max(TINT_SEC,rtt_avg_*4);

    tint timed_out = NOW - plan_for*2;
    while ( !hint_out_.empty() && hint_out_.front().time < timed_out ) {
        hint_out_size_ -= hint_out_.front().bin.width();
        hint_out_.pop_front();
    }

    int plan_pck = max ( (tint)1, plan_for / dip_avg_ );

    if ( hint_out_size_ < plan_pck ) {

        int diff = plan_pck - hint_out_size_; // TODO: aggregate
        bin64_t hint = transfer().picker().Pick(ack_in_,diff,NOW+plan_for*2);

        if (hint!=bin64_t::NONE) {
            evbuffer_add_8(evb, SWIFT_HINT);
            evbuffer_add_32be(evb, hint);
            dprintf("%s #%u +hint %s [%lli]\n",tintstr(),id_,hint.str(),hint_out_size_);
            hint_out_.push_back(hint);
            hint_out_size_ += hint.width();
        } else
            dprintf("%s #%u Xhint\n",tintstr(),id_);

    }
}


bin64_t        Channel::AddData (struct evbuffer *evb) {

    if (!file().size()) // know nothing
        return bin64_t::NONE;

    bin64_t tosend = bin64_t::NONE;
    tint luft = send_interval_>>4; // may wake up a bit earlier
    if (data_out_.size()<cwnd_ &&
            last_data_out_time_+send_interval_<=NOW+luft) {
        tosend = DequeueHint();
        if (tosend==bin64_t::NONE) {
            dprintf("%s #%u sendctrl no idea what to send\n",tintstr(),id_);
            if (send_control_!=KEEP_ALIVE_CONTROL)
                SwitchSendControl(KEEP_ALIVE_CONTROL);
        }
    } else
        dprintf("%s #%u sendctrl wait cwnd %f data_out %i next %s\n",
                tintstr(),id_,cwnd_,(int)data_out_.size(),tintstr(last_data_out_time_+NOW-send_interval_));

    if (tosend==bin64_t::NONE)// && (last_data_out_time_>NOW-TINT_SEC || data_out_.empty()))
        return bin64_t::NONE; // once in a while, empty data is sent just to check rtt FIXED

    if (ack_in_.is_empty() && file().size())
        AddPeakHashes(evb);
    AddUncleHashes(evb,tosend);
    if (!ack_in_.is_empty()) // TODO: cwnd_>1
        data_out_cap_ = tosend;

    if (evbuffer_get_length(evb)>254) {
	Channel::SendTo(socket_,peer(),evb); // kind of fragmentation
        evbuffer_add_32be(evb, peer_channel_id_);
    }

    evbuffer_add_8(evb, SWIFT_DATA);
    evbuffer_add_32be(evb, tosend.to32());

    uint8_t buf[1024];
    size_t r = pread(file().file_descriptor(),buf,1024,tosend.base_offset()<<10);
    // TODO: corrupted data, retries, caching
    if (r<0) {
        print_error("error on reading");
        return bin64_t::NONE;
    }
    // assert(dgram.space()>=r+4+1);
    evbuffer_add(evb, buf, r);

    last_data_out_time_ = NOW;
    data_out_.push_back(tosend);
    dprintf("%s #%u +data %s\n",tintstr(),id_,tosend.str());

    return tosend;
}


void    Channel::AddAck (struct evbuffer *evb) {
    if (data_in_==tintbin())
        return;
    evbuffer_add_8(evb, SWIFT_ACK);
    evbuffer_add_32be(evb, data_in_.bin.to32()); // FIXME not cover
    evbuffer_add_64be(evb, data_in_.time); // FIXME 32
    have_out_.set(data_in_.bin);
    dprintf("%s #%u +ack %s %s\n",
        tintstr(),id_,data_in_.bin.str(),tintstr(data_in_.time));
    if (data_in_.bin.layer()>2)
        data_in_dbl_ = data_in_.bin;
    data_in_ = tintbin();
}


void    Channel::AddHave (struct evbuffer *evb) {
    if (data_in_dbl_!=bin64_t::NONE) { // TODO: do redundancy better
        evbuffer_add_8(evb, SWIFT_HAVE);
        evbuffer_add_32be(evb, data_in_dbl_.to32());
        data_in_dbl_=bin64_t::NONE;
    }
    for(int count=0; count<4; count++) {
        bin64_t ack = file().ack_out().find_filtered // FIXME: do rotating queue
            (have_out_, bin64_t::ALL, binmap_t::FILLED);
        if (ack==bin64_t::NONE)
            break;
        ack = file().ack_out().cover(ack);
        have_out_.set(ack);
        evbuffer_add_8(evb, SWIFT_HAVE);
        evbuffer_add_32be(evb, ack.to32());
        dprintf("%s #%u +have %s\n",tintstr(),id_,ack.str());
    }
}


void    Channel::Recv (struct evbuffer *evb) {
    dprintf("%s #%u recvd %ib\n",tintstr(),id_,evbuffer_get_length(evb)+4);
    dgrams_rcvd_++;
    if (last_send_time_ && rtt_avg_==TINT_SEC && dev_avg_==0) {
        rtt_avg_ = NOW - last_send_time_;
        dev_avg_ = rtt_avg_;
        dip_avg_ = rtt_avg_;
        dprintf("%s #%u sendctrl rtt init %lli\n",tintstr(),id_,rtt_avg_);
    }
    bin64_t data = evbuffer_get_length(evb) ? bin64_t::NONE : bin64_t::ALL;
    while (evbuffer_get_length(evb)) {
        uint8_t type = evbuffer_remove_8(evb);
        switch (type) {
            case SWIFT_HANDSHAKE: OnHandshake(evb); break;
            case SWIFT_DATA:      data=OnData(evb); break;
            case SWIFT_HAVE:      OnHave(evb); break;
            case SWIFT_ACK:       OnAck(evb); break;
            case SWIFT_HASH:      OnHash(evb); break;
            case SWIFT_HINT:      OnHint(evb); break;
            case SWIFT_PEX_ADD:   OnPex(evb); break;
            default:
                eprintf("%s #%u ?msg id unknown %i\n",tintstr(),id_,(int)type);
                return;
        }
    }
    last_recv_time_ = NOW;
    sent_since_recv_ = 0;
    Reschedule();
}


void    Channel::OnHash (struct evbuffer *evb) {
    bin64_t pos = evbuffer_remove_32be(evb);
    Sha1Hash hash = evbuffer_remove_hash(evb);
    file().OfferHash(pos,hash);
    dprintf("%s #%u -hash %s\n",tintstr(),id_,pos.str());
}


void    Channel::CleanHintOut (bin64_t pos) {
    int hi = 0;
    while (hi<hint_out_.size() && !pos.within(hint_out_[hi].bin))
        hi++;
    if (hi==hint_out_.size())
        return; // something not hinted or hinted in far past
    while (hi--) { // removing likely snubbed hints
        hint_out_size_ -= hint_out_.front().bin.width();
        hint_out_.pop_front();
    }
    while (hint_out_.front().bin!=pos) {
        tintbin f = hint_out_.front();
        f.bin = f.bin.towards(pos);
        hint_out_.front().bin = f.bin.sibling();
        hint_out_.push_front(f);
    }
    hint_out_.pop_front();
    hint_out_size_--;
}


bin64_t Channel::OnData (struct evbuffer *evb) {  // TODO: HAVE NONE for corrupted data
    bin64_t pos = evbuffer_remove_32be(evb);
    int length = (evbuffer_get_length(evb) < 1024) ? evbuffer_get_length(evb)
	: 1024;
    uint8_t *data = evbuffer_pullup(evb, length);
    bool ok = (pos==bin64_t::NONE) || 
        (!file().ack_out().get(pos) && file().OfferData(pos, (char*)data, length) );
    evbuffer_drain(evb, length);
    dprintf("%s #%u %cdata %s\n",tintstr(),id_,ok?'-':'!',pos.str());
    data_in_ = tintbin(NOW,bin64_t::NONE);
    if (!ok)
        return bin64_t::NONE;
    bin64_t cover = transfer().ack_out().cover(pos);
    for(int i=0; i<transfer().cb_installed; i++)
        if (cover.layer()>=transfer().cb_agg[i])
            transfer().callbacks[i](transfer().fd(),cover);  // FIXME
    data_in_.bin = pos;
    if (pos!=bin64_t::NONE) {
        if (last_data_in_time_) {
            tint dip = NOW - last_data_in_time_;
            dip_avg_ = ( dip_avg_*3 + dip ) >> 2;
        }
        last_data_in_time_ = NOW;
    }
    CleanHintOut(pos);
    return pos;
}


void    Channel::OnAck (struct evbuffer *evb) {
    bin64_t ackd_pos = evbuffer_remove_32be(evb);
    tint peer_time = evbuffer_remove_64be(evb); // FIXME 32
    // FIXME FIXME: wrap around here
    if (ackd_pos==bin64_t::NONE)
        return; // likely, brocken packet / insufficient hashes
    if (file().size() && ackd_pos.base_offset()>=file().packet_size()) {
        eprintf("invalid ack: %s\n",ackd_pos.str());
        return;
    }
    ack_in_.set(ackd_pos);
    int di = 0, ri = 0;
    // find an entry for the send (data out) event
    while (  di<data_out_.size() && ( data_out_[di]==tintbin() ||
           !data_out_[di].bin.within(ackd_pos) )  )
        di++;
    // FUTURE: delayed acks
    // rule out retransmits
    while (  ri<data_out_tmo_.size() && !data_out_tmo_[ri].bin.within(ackd_pos) )
        ri++;
    dprintf("%s #%u %cack %s %lli\n",tintstr(),id_,
            di==data_out_.size()?'?':'-',ackd_pos.str(),peer_time);
    if (di!=data_out_.size() && ri==data_out_tmo_.size()) { // not a retransmit
            // round trip time calculations
        tint rtt = NOW-data_out_[di].time;
        rtt_avg_ = (rtt_avg_*7 + rtt) >> 3;
        dev_avg_ = ( dev_avg_*3 + ::abs(rtt-rtt_avg_) ) >> 2;
        assert(data_out_[di].time!=TINT_NEVER);
            // one-way delay calculations
        tint owd = peer_time - data_out_[di].time;
        owd_cur_bin_ = 0;//(owd_cur_bin_+1) & 3;
        owd_current_[owd_cur_bin_] = owd;
        if ( owd_min_bin_start_+TINT_SEC*30 < NOW ) {
            owd_min_bin_start_ = NOW;
            owd_min_bin_ = (owd_min_bin_+1) & 3;
            owd_min_bins_[owd_min_bin_] = TINT_NEVER;
        }
        if (owd_min_bins_[owd_min_bin_]>owd)
            owd_min_bins_[owd_min_bin_] = owd;
        dprintf("%s #%u sendctrl rtt %lli dev %lli based on %s\n",
                tintstr(),id_,rtt_avg_,dev_avg_,data_out_[di].bin.str());
        ack_rcvd_recent_++;
        // early loss detection by packet reordering
        for (int re=0; re<di-MAX_REORDERING; re++) {
            if (data_out_[re]==tintbin())
                continue;
            ack_not_rcvd_recent_++;
            data_out_tmo_.push_back(data_out_[re].bin);
            dprintf("%s #%u Rdata %s\n",tintstr(),id_,data_out_.front().bin.str());
            data_out_cap_ = bin64_t::ALL;
            data_out_[re] = tintbin();
        }
    }
    if (di!=data_out_.size())
        data_out_[di]=tintbin();
    // clear zeroed items
    while (!data_out_.empty() && ( data_out_.front()==tintbin() ||
            ack_in_.is_filled(data_out_.front().bin) ) )
        data_out_.pop_front();
    assert(data_out_.empty() || data_out_.front().time!=TINT_NEVER);
}


void Channel::TimeoutDataOut ( ) {
    // losses: timeouted packets
    tint timeout = NOW - ack_timeout();
    while (!data_out_.empty() && 
        ( data_out_.front().time<timeout || data_out_.front()==tintbin() ) ) {
        if (data_out_.front()!=tintbin() && ack_in_.is_empty(data_out_.front().bin)) {
            ack_not_rcvd_recent_++;
            data_out_cap_ = bin64_t::ALL;
            data_out_tmo_.push_back(data_out_.front().bin);
            dprintf("%s #%u Tdata %s\n",tintstr(),id_,data_out_.front().bin.str());
        }
        data_out_.pop_front();
    }
    // clear retransmit queue of older items
    while (!data_out_tmo_.empty() && data_out_tmo_.front().time<NOW-MAX_POSSIBLE_RTT)
        data_out_tmo_.pop_front();
}


void Channel::OnHave (struct evbuffer *evb) {
    bin64_t ackd_pos = evbuffer_remove_32be(evb);
    if (ackd_pos==bin64_t::NONE)
        return; // wow, peer has hashes
    ack_in_.set(ackd_pos);
    dprintf("%s #%u -have %s\n",tintstr(),id_,ackd_pos.str());
}


void    Channel::OnHint (struct evbuffer *evb) {
    bin64_t hint = evbuffer_remove_32be(evb);
    // FIXME: wake up here
    hint_in_.push_back(hint);
    dprintf("%s #%u -hint %s\n",tintstr(),id_,hint.str());
}


void Channel::OnHandshake (struct evbuffer *evb) {
    peer_channel_id_ = evbuffer_remove_32be(evb);
    dprintf("%s #%u -hs %x\n",tintstr(),id_,peer_channel_id_);
    // self-connection check
    if (!SELF_CONN_OK) {
        uint32_t try_id = DecodeID(peer_channel_id_);
        if (channel(try_id) && !channel(try_id)->peer_channel_id_) {
            peer_channel_id_ = 0;
            Close();
            return; // this is a self-connection
        }
    }
    // FUTURE: channel forking
}


void Channel::OnPex (struct evbuffer *evb) {
    uint32_t ipv4 = evbuffer_remove_32be(evb);
    uint16_t port = evbuffer_remove_16be(evb);
    Address addr(ipv4,port);
    dprintf("%s #%u -pex %s\n",tintstr(),id_,addr.str());
    transfer().OnPexIn(addr);
}


void    Channel::AddPex (struct evbuffer *evb) {
    int chid = transfer().RevealChannel(pex_out_);
    if (chid==-1 || chid==id_)
        return;
    Address a = channels[chid]->peer();
    evbuffer_add_8(evb, SWIFT_PEX_ADD);
    evbuffer_add_32be(evb, a.ipv4());
    evbuffer_add_16be(evb, a.port());
    dprintf("%s #%u +pex %s\n",tintstr(),id_,a.str());
}


void    Channel::RecvDatagram (SOCKET socket) {
    // Datagram data(socket);
    // data.Recv();
    // const Address& addr = data.address();
    struct evbuffer *evb = evbuffer_new();
    Address addr;
    RecvFrom(socket, addr, evb);
#define return_log(...) { fprintf(stderr,__VA_ARGS__); return; }
    if (evbuffer_get_length(evb)<4)
        return_log("datagram shorter than 4 bytes %s\n",addr.str());
    uint32_t mych = evbuffer_remove_32be(evb);
    Sha1Hash hash;
    Channel* channel = NULL;
    if (mych==0) { // handshake initiated
        if (evbuffer_get_length(evb)<1+4+1+4+Sha1Hash::SIZE)
            return_log ("%s #0 incorrect size %i initial handshake packet %s\n",
                        tintstr(),evbuffer_get_length(evb),addr.str());
        uint8_t hashid = evbuffer_remove_8(evb);
        if (hashid!=SWIFT_HASH)
            return_log ("%s #0 no hash in the initial handshake %s\n",
                        tintstr(),addr.str());
        bin64_t pos = evbuffer_remove_32be(evb);
        if (pos!=bin64_t::ALL)
            return_log ("%s #0 that is not the root hash %s\n",tintstr(),addr.str());
        hash = evbuffer_remove_hash(evb);
        FileTransfer* file = FileTransfer::Find(hash);
        if (!file)
            return_log ("%s #0 hash %s unknown, no such file %s\n",tintstr(),hash.hex().c_str(),addr.str());
        dprintf("%s #0 -hash ALL %s\n",tintstr(),hash.hex().c_str());
        for(binqueue::iterator i=file->hs_in_.begin(); i!=file->hs_in_.end(); i++)
            if (channels[*i] && channels[*i]->peer_==addr &&
                channels[*i]->last_recv_time_>NOW-TINT_SEC*2)
                return_log("%s #0 have a channel already to %s\n",tintstr(),addr.str());
        channel = new Channel(file, socket, addr);
    } else {
        mych = DecodeID(mych);
        if (mych>=channels.size())
            return_log("%s invalid channel #%u, %s\n",tintstr(),mych,addr.str());
        channel = channels[mych];
        if (!channel)
            return_log ("%s #%u is already closed\n",tintstr(),mych,addr.str());
        if (channel->peer() != addr)
            return_log ("%s #%u invalid peer address %s!=%s\n",
                        tintstr(),mych,channel->peer().str(),addr.str());
        channel->own_id_mentioned_ = true;
    }
    //dprintf("recvd %i bytes for %i\n",data.size(),channel->id);
    channel->Recv(evb);
}



void Channel::Close () {
    this->SwitchSendControl(CLOSE_CONTROL);
}


void Channel::Reschedule () {
    next_send_time_ = NextSendTime();
    if (next_send_time_!=TINT_NEVER) {
        assert(next_send_time_<NOW+TINT_MIN);
        //send_queue.push(tintbin(next_send_time_,id_));
        evtimer_del(&evsend);
        evtimer_add(&evsend,tint2tv(next_send_time_-NOW));
        dprintf("%s #%u requeue for %s\n",tintstr(),id_,tintstr(next_send_time_));
    } else {
        dprintf("%s #%u closed\n",tintstr(),id_);
        delete this;
    }
}

 
void Channel::SendCallback(int fd, short event, void *arg) {
    dprintf(">scb\n");
    Time();
    Channel * sender = (Channel*) arg;
    if (NOW<sender->next_send_time_-TINT_MSEC)
        dprintf("%s #%u suspicious send %s<%s\n",tintstr(),
                sender->id(),tintstr(NOW),tintstr(sender->next_send_time_));
    sender->Send();
    dprintf("<scb\n");
}

void Channel::ReceiveCallback(int fd, short event, void *arg) {
    dprintf("rcb\n");
    Time();
    RecvDatagram(fd);
    event_add(&evrecv, NULL);
}
