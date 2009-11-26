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
 - range: ALL
 - randomized testing of advanced ops (new testcase)
 */

void    Channel::AddPeakHashes (Datagram& dgram) {
    for(int i=0; i<file().peak_count(); i++) {
        bin64_t peak = file().peak(i);
        dgram.Push8(P2TP_HASH);
        dgram.Push32((uint32_t)peak);
        dgram.PushHash(file().peak_hash(i));
        //DLOG(INFO)<<"#"<<id<<" +pHASH"<<file().peak(i);
        dprintf("%s #%i +phash %s\n",tintstr(),id,peak.str());
    }
}


void    Channel::AddUncleHashes (Datagram& dgram, bin64_t pos) {
    bin64_t peak = file().peak_for(pos);
    while (pos!=peak && ((NOW&3)==3 || !data_out_cap_.within(pos.parent())) &&
            ack_in_.get(pos.parent())==bins::EMPTY) {
        bin64_t uncle = pos.sibling();
        dgram.Push8(P2TP_HASH);
        dgram.Push32((uint32_t)uncle);
        dgram.PushHash( file().hash(uncle) );
        //DLOG(INFO)<<"#"<<id<<" +uHASH"<<uncle;
        dprintf("%s #%i +hash %s\n",tintstr(),id,uncle.str());
        pos = pos.parent();
    }
}


bin64_t        Channel::DequeueHint () { // TODO: resilience
    bin64_t send = bin64_t::NONE;
    while (!hint_in_.empty() && send==bin64_t::NONE) {
        bin64_t hint = hint_in_.front().bin;
        tint time = hint_in_.front().time;
        hint_in_.pop_front();
        //if (time < NOW-TINT_SEC*3/2 ) //NOW-8*rtt_avg_)
        //    continue;
        // Totally flawed:
        // a. May empty the queue when you least expect
        // b. May lose parts of partially ACKd HINTs
        send = file().ack_out().find_filtered(ack_in_,hint,bins::FILLED);
        send = send.left_foot(); // single packet
        if (send!=bin64_t::NONE)
            while (send!=hint) {
                hint = hint.towards(send);
                hint_in_.push_front(hint.sibling());
            }
    }
    uint64_t mass = 0;
    for(int i=0; i<hint_in_.size(); i++)
        mass += hint_in_[i].bin.width();
    dprintf("%s #%i dequeued %s [%lli]\n",tintstr(),id,send.str(),mass);
    return send;
}


void    Channel::AddHandshake (Datagram& dgram) {
    if (!peer_channel_id_) { // initiating
        dgram.Push8(P2TP_HASH);
        dgram.Push32(bin64_t::ALL32);
        dgram.PushHash(file().root_hash());
        dprintf("%s #%i +hash ALL %s\n",
                tintstr(),id,file().root_hash().hex().c_str());
    }
    dgram.Push8(P2TP_HANDSHAKE);
    int encoded = EncodeID(id);
    dgram.Push32(encoded);
    dprintf("%s #%i +hs %i\n",tintstr(),id,encoded);
    ack_out_.clear();
    AddAck(dgram);
}


void    Channel::Send () {
    Datagram dgram(socket_,peer());
    dgram.Push32(peer_channel_id_);
    bin64_t data = bin64_t::NONE;
    if ( is_established() ) {
        // FIXME: seeder check
        AddAck(dgram);
        if (!file().is_complete())
            AddHint(dgram);
        AddPex(dgram);
        CleanDataOut();
        data = AddData(dgram);
    } else {
        AddHandshake(dgram);
        AddAck(dgram);
    }
    dprintf("%s #%i sent %ib %s\n",tintstr(),id,dgram.size(),peer().str());
    if (dgram.size()==4) // only the channel id; bare keep-alive
        data = bin64_t::ALL;
    cc_->OnDataSent(data);
    if (dgram.Send()==-1)
        print_error("can't send datagram");
}


void    Channel::AddHint (Datagram& dgram) {

    tint timed_out = NOW - TINT_SEC*3/2;
    while ( !hint_out_.empty() && hint_out_.front().time < timed_out ) {
        hint_out_size_ -= hint_out_.front().bin.width();
        hint_out_.pop_front();
    }
    
    int peer_cwnd = (int)(rtt_avg_ / dip_avg_);
    if (!peer_cwnd)
        peer_cwnd = 1;
    int peer_pps = TINT_SEC / dip_avg_; // data packets per sec
    if (!peer_pps)
        peer_pps = 1;
    
    if ( hint_out_size_ < peer_pps ) { //4*peer_cwnd ) {
            
        int diff = peer_pps - hint_out_size_;
        //if (diff>4 && diff>2*peer_cwnd)
        //    diff >>= 1;
        bin64_t hint = transfer().picker().Pick(ack_in_,diff,rtt_avg_*8+TINT_MSEC*100);
        
        if (hint!=bin64_t::NONE) {
            dgram.Push8(P2TP_HINT);
            dgram.Push32(hint);
            dprintf("%s #%i +hint %s [%lli]\n",tintstr(),id,hint.str(),hint_out_size_);
            hint_out_.push_back(hint);
            hint_out_size_ += hint.width();
        } else
            dprintf("%s #%i .hint\n",tintstr(),id);
        
    }
}


bin64_t        Channel::AddData (Datagram& dgram) {
    
    if (!file().size()) // know nothing
        return bin64_t::NONE;
    
    bin64_t tosend = bin64_t::NONE;
    if (cc_->MaySendData()) {
        tosend = DequeueHint();
        if (tosend==bin64_t::NONE)
            dprintf("%s #%i out of hints #sendctrl\n",tintstr(),id);
    } else
        dprintf("%s #%i no cwnd #sendctrl\n",tintstr(),id);
    
    if (tosend==bin64_t::NONE && (last_send_data_time_>NOW-TINT_SEC || data_out_.empty())) 
        return bin64_t::NONE; // once in a while, empty data is sent just to check rtt
    
    if (tosend!=bin64_t::NONE) { // hashes
        if (ack_in_.is_empty() && file().size())
            AddPeakHashes(dgram);
        AddUncleHashes(dgram,tosend);
        if (!ack_in_.is_empty()) // TODO: cwnd_>1
            data_out_cap_ = tosend;
    }

    if (dgram.size()>254) {
        dgram.Send(); // kind of fragmentation
        dgram.Push32(peer_channel_id_);
    }
    
    dgram.Push8(P2TP_DATA);
    dgram.Push32(tosend.to32());
    
    if (tosend!=bin64_t::NONE) { // data
        uint8_t buf[1024];
        size_t r = pread(file().file_descriptor(),buf,1024,tosend.base_offset()<<10); 
        // TODO: corrupted data, retries, caching
        if (r<0) {
            print_error("error on reading");
            return bin64_t::NONE;
        }
        assert(dgram.space()>=r+4+1);
        dgram.Push(buf,r);
    }
    
    last_send_data_time_ = NOW;
    data_out_.push_back(tosend);
    dprintf("%s #%i +data %s\n",tintstr(),id,tosend.str());
    
    return tosend;
}


void    Channel::AddTs (Datagram& dgram) {
    dgram.Push8(P2TP_TS);
    dgram.Push64(data_in_.time);
    dprintf("%s #%i +ts %s\n",tintstr(),id,tintstr(data_in_.time));
}


void    Channel::AddAck (Datagram& dgram) {
    if (data_in_dbl_!=bin64_t::NONE) {
        dgram.Push8(P2TP_ACK);
        dgram.Push32(data_in_dbl_);
        data_in_dbl_=bin64_t::NONE;
    }
    if (data_in_.bin!=bin64_t::NONE) {
        AddTs(dgram);
        bin64_t pos = file().ack_out().cover(data_in_.bin);
        dgram.Push8(P2TP_ACK);
        dgram.Push32(pos);
        //dgram.Push64(data_in_.time);
        ack_out_.set(pos);
        dprintf("%s #%i +ack %s %s\n",tintstr(),id,pos.str(),tintstr(data_in_.time));
        data_in_ = tintbin(0,bin64_t::NONE);
        if (pos.layer()>2)
            data_in_dbl_ = pos;
    }
    for(int count=0; count<4; count++) {
        bin64_t ack = file().ack_out().find_filtered(ack_out_, bin64_t::ALL, bins::FILLED);
        if (ack==bin64_t::NONE)
            break;
        ack = file().ack_out().cover(ack);
        ack_out_.set(ack);
        dgram.Push8(P2TP_ACK);
        dgram.Push32(ack);
        dprintf("%s #%i +ack %s\n",tintstr(),id,ack.str());
    }
}


void    Channel::Recv (Datagram& dgram) {
    dprintf("%s #%i recvd %i\n",tintstr(),id,dgram.size()+4);
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
            case P2TP_DATA:        data=OnData(dgram); break;
            case P2TP_TS:       OnTs(dgram); break;
            case P2TP_ACK:        OnAck(dgram); break;
            case P2TP_HASH:        OnHash(dgram); break;
            case P2TP_HINT:        OnHint(dgram); break;
            case P2TP_PEX_ADD:  OnPex(dgram); break;
            default:
                eprintf("%s #%i ?msg id unknown %i\n",tintstr(),id,(int)type);
                return;
        }
    }
    cc_->OnDataRecvd(data);
    last_recv_time_ = NOW;
}


void    Channel::OnHash (Datagram& dgram) {
    bin64_t pos = dgram.Pull32();
    Sha1Hash hash = dgram.PullHash();
    file().OfferHash(pos,hash);
    //DLOG(INFO)<<"#"<<id<<" .HASH"<<(int)pos;
    dprintf("%s #%i -hash %s\n",tintstr(),id,pos.str());
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


bin64_t Channel::OnData (Datagram& dgram) {
    bin64_t pos = dgram.Pull32();
    uint8_t *data;
    int length = dgram.Pull(&data,1024);
    bool ok = (pos==bin64_t::NONE) || file().OfferData(pos, (char*)data, length) ;
    dprintf("%s #%i %cdata %s\n",tintstr(),id,ok?'-':'!',pos.str());
    if (!ok) 
        return bin64_t::NONE;
    data_in_ = tintbin(NOW,pos);
    if (pos!=bin64_t::NONE) {
        if (last_recv_data_time_) {
            tint dip = NOW - last_recv_data_time_;
            dip_avg_ = ( dip_avg_*3 + dip ) >> 2;
        }
        last_recv_data_time_ = NOW;
    }
    CleanHintOut(pos);    
    return pos;
}


void    Channel::CleanDataOut (bin64_t ackd_pos) {
    
    int max_ack_off = 0;
    
    if (ackd_pos!=bin64_t::NONE) {
        for (int i=0; i<8 && i<data_out_.size(); i++) {
            if (data_out_[i]!=tintbin() && data_out_[i].bin.within(ackd_pos)) {
                tint rtt = NOW-data_out_[i].time;
                rtt_avg_ = (rtt_avg_*7 + rtt) >> 3;
                dev_avg_ = ( dev_avg_*3 + abs(rtt-rtt_avg_) ) >> 2;
                dprintf("%s #%i rtt %lli dev %lli\n",tintstr(),id,rtt_avg_,dev_avg_);
                bin64_t pos = data_out_[i].bin;
                cc_->OnAckRcvd(pos);
                data_out_[i]=tintbin();
                max_ack_off = i;
                if (ackd_pos==pos)
                    break;
            }
        }
        while (!data_out_.empty() && data_out_.front().bin==bin64_t::NONE) {
            data_out_.pop_front();
            max_ack_off--;
        }
        static const int MAX_REORDERING = 2;  // the triple-ACK principle
        if (max_ack_off>MAX_REORDERING) {
            while (max_ack_off && (data_out_.front().bin==bin64_t::NONE
                                   || ack_in_.is_filled(data_out_.front().bin)) ) {
                data_out_.pop_front();
                max_ack_off--;
            }
            while (max_ack_off>MAX_REORDERING) {
                cc_->OnAckRcvd(bin64_t::NONE);
                data_out_.pop_front();
                max_ack_off--;
                data_out_cap_ = bin64_t::ALL;
                dprintf("%s #%i Rdata %s\n",tintstr(),id,data_out_.front().bin.str());
            }
        }
    }
    tint timeout = NOW - rtt_avg_ - 4*std::max(dev_avg_,TINT_MSEC*50);
    while (!data_out_.empty() && data_out_.front().time<timeout) {
        if (data_out_.front().bin!=bin64_t::NONE && ack_in_.is_empty(data_out_.front().bin)) {
            cc_->OnAckRcvd(bin64_t::NONE);
            data_out_cap_ = bin64_t::ALL;
            dprintf("%s #%i Tdata %s\n",tintstr(),id,data_out_.front().bin.str());
        }
        data_out_.pop_front();
    }
    while (!data_out_.empty() && data_out_.front().bin==bin64_t::NONE)
        data_out_.pop_front();

}


void    Channel::OnAck (Datagram& dgram) {
    bin64_t ackd_pos = dgram.Pull32();
    if (ackd_pos!=bin64_t::NONE && file().size() && ackd_pos.base_offset()>=file().packet_size()) {
        eprintf("invalid ack: %s\n",ackd_pos.str());
        return;
    }
    dprintf("%s #%i -ack %s\n",tintstr(),id,ackd_pos.str());
    ack_in_.set(ackd_pos);
    CleanDataOut(ackd_pos);
}


void Channel::OnTs (Datagram& dgram) {
    peer_send_time_ = dgram.Pull64();
    dprintf("%s #%i -ts %lli\n",tintstr(),id,peer_send_time_);
}


void    Channel::OnHint (Datagram& dgram) {
    bin64_t hint = dgram.Pull32();
    hint_in_.push_back(hint);
    //ack_in_.set(hint,bins::EMPTY);
    //RequeueSend(cc_->OnHintRecvd(hint));
    dprintf("%s #%i -hint %s\n",tintstr(),id,hint.str());
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
    dprintf("%s #%i -pex %s\n",tintstr(),id,addr.str());
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
    dprintf("%s #%i +pex %s\n",tintstr(),id,a.str());
}


void    Channel::RecvDatagram (int socket) {
    Datagram data(socket);
    data.Recv();
    Address& addr = data.addr;
#define return_log(...) { eprintf(__VA_ARGS__); return; }
    if (data.size()<4) 
        return_log("datagram shorter than 4 bytes %s\n",addr.str());
    uint32_t mych = data.Pull32();
    Sha1Hash hash;
    Channel* channel = NULL;
    if (!mych) { // handshake initiated
        if (data.size()<1+4+1+4+Sha1Hash::SIZE) 
            return_log ("incorrect size %i initial handshake packet %s\n",data.size(),addr.str());
        uint8_t hashid = data.Pull8();
        if (hashid!=P2TP_HASH) 
            return_log ("no hash in the initial handshake %s\n",addr.str());
        bin64_t pos = data.Pull32();
        if (pos!=bin64_t::ALL) 
            return_log ("that is not the root hash %s\n",addr.str());
        hash = data.PullHash();
        FileTransfer* file = FileTransfer::Find(hash);
        if (!file) 
            return_log ("hash %s unknown, no such file %s\n",hash.hex().c_str(),addr.str());
        dprintf("%s #0 -hash ALL %s\n",tintstr(),hash.hex().c_str());
        for(binqueue::iterator i=file->hs_in_.begin(); i!=file->hs_in_.end(); i++)
            if (channels[*i] && channels[*i]->peer_==data.addr && 
                channels[*i]->last_recv_time_>NOW-TINT_SEC*2) 
                return_log("have a channel already to %s\n",addr.str());
        channel = new Channel(file, socket, data.address());
    } else {
        mych = DecodeID(mych);
        if (mych>=channels.size()) 
            return_log("invalid channel #%i, %s\n",mych,addr.str());
        channel = channels[mych];
        if (!channel) 
            return_log ("channel #%i is already closed\n",mych,addr.str());
        if (channel->peer() != addr) 
            return_log ("invalid peer address #%i %s!=%s\n",mych,channel->peer().str(),addr.str());
        channel->own_id_mentioned_ = true;
    }
    //dprintf("recvd %i bytes for %i\n",data.size(),channel->id);
    channel->Recv(data);
}


void    Channel::Loop (tint howlong) {  
    
    tint limit = Datagram::Time() + howlong;
    
    do {

        tint send_time(TINT_NEVER);
        Channel* sender(NULL);
        while (!send_queue.is_empty()) {
            send_time = send_queue.peek().time;
            sender = channel((int)send_queue.peek().bin);
            if (sender)
                if ( sender->next_send_time_==send_time ||
                     sender->next_send_time_==TINT_NEVER )
                break;
            sender = NULL; // it was a stale entry
            send_queue.pop();
        }
        if (send_time>limit)
            send_time = limit;
        if ( sender && sender->next_send_time_ <= NOW ) {
            dprintf("%s #%i sch_send %s\n",tintstr(),sender->id,
                    tintstr(send_time));
            sender->Send();
            sender->last_send_time_ = NOW;
            // sender->RequeueSend(sender->cc_->NextSendTime()); goes to SendCtrl
            send_queue.pop();
        } else if ( send_time > NOW ) {
            tint towait = send_time - NOW;
            dprintf("%s waiting %lliusec\n",tintstr(),towait);
            int rd = Datagram::Wait(socket_count,sockets,towait);
            if (rd!=INVALID_SOCKET)
                RecvDatagram(rd);
        } else  { // FIXME FIXME FIXME REWRITE!!!  if (sender->next_send_time_==TINT_NEVER) { 
            if (sender) {
            dprintf("%s #%i closed sendctrl\n",tintstr(),sender->id);
            delete sender;
            }
            send_queue.pop();
        }
        
    } while (Datagram::Time()<limit);
        
}

 
void Channel::Schedule (tint next_time) {
    next_send_time_ = next_time;
    if (next_time==TINT_NEVER)
        next_time = NOW + TINT_MIN; // 1min timeout
    send_queue.push(tintbin(next_time,id));
    dprintf("%s requeue #%i for %s\n",tintstr(),id,tintstr(next_time));
}
