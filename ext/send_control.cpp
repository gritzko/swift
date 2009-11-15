/*
 *  send_control.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 11/4/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "p2tp.h"


using namespace p2tp;


void    SendController::Swap (SendController* newctrl) {
    dprintf("%s #%i sendctrl %s->%s\n",tintstr(),ch_->id,type(),newctrl->type());
    assert(this==ch_->cc_);
    ch_->cc_ = newctrl;
    delete this;
}


void    SendController::Schedule (tint next_time) {
    ch_->Schedule(next_time);
}

bool    PingPongController::MaySendData() {
    return ch_->data_out_.empty();
}

void    PingPongController::OnDataSent(bin64_t b) {
    Schedule(NOW+ch_->rtt_avg_+std::max(ch_->dev_avg_*4,500*TINT_MSEC));
    if (++sent_>=10 || ++unanswered_>=3)
        Swap(new KeepAliveController(this));
}

void    PingPongController::OnDataRecvd(bin64_t b) {
    unanswered_ = 0;
    Schedule(NOW); // pong
}

void    PingPongController::OnAckRcvd(bin64_t ackd) {
    if (ackd!=bin64_t::NONE) {
        Schedule(NOW);
        Swap(new SlowStartController(this));
    }
}


KeepAliveController::KeepAliveController (Channel* ch) : SendController(ch), delay_(ch->rtt_avg_) {
}


KeepAliveController::KeepAliveController(SendController* prev, tint delay) : 
SendController(prev), delay_(delay) {
    ch_->dev_avg_ = TINT_SEC; // without constant active measurement, rtt is unreliable
    delay_=ch_->rtt_avg_;
}

bool    KeepAliveController::MaySendData() {
    return true;
}
    

void    KeepAliveController::OnDataSent(bin64_t b) {
    if (b==bin64_t::ALL || b==bin64_t::NONE) {
        if (delay_>TINT_SEC*58) // keep NAT mappings alive
            delay_ = TINT_SEC*58;
        if (delay_>=4*TINT_SEC && ch_->last_recv_time_ < NOW-TINT_MIN)
            Schedule(TINT_NEVER); // no response; enter close timeout
        else
            Schedule(NOW+delay_); // all right, just keep it alive
        delay_ = delay_ * 2; // backing off
    } else {
        Schedule(NOW+ch_->rtt_avg_); // cwnd==1 => next send in 1 rtt
        Swap(new SlowStartController(this));
    }
}
    
void    KeepAliveController::OnDataRecvd(bin64_t b) {
    if (b!=bin64_t::NONE && b!=bin64_t::ALL) { // channel is alive
        delay_ = ch_->rtt_avg_;
        Schedule(NOW); // schedule an ACK; TODO: aggregate
    }
}
    
void    KeepAliveController::OnAckRcvd(bin64_t ackd) {
    // probably to something sent by CwndControllers before this one got installed
}
    

CwndController::CwndController(SendController* orig, int cwnd) :
SendController(orig), cwnd_(cwnd), last_change_(0) {    
}

bool    CwndController::MaySendData() {
    tint spacing = ch_->rtt_avg_ / cwnd_;
    dprintf("%s #%i sendctrl may send %i < %f & %s (rtt %lli)\n",tintstr(),
            ch_->id,(int)ch_->data_out_.size(),cwnd_,
            tintstr(ch_->last_send_data_time_+spacing), ch_->rtt_avg_);
    return  ch_->data_out_.empty() ||
            (ch_->data_out_.size() < cwnd_  &&  NOW-ch_->last_send_data_time_ >= spacing);
}
    

void    CwndController::OnDataSent(bin64_t b) {
    if ( (b==bin64_t::ALL || b==bin64_t::NONE) && MaySendData() ) { // no more data (no hints?)
        Schedule(NOW+ch_->rtt_avg_); // soft pause; nothing to send yet
        if (ch_->last_send_data_time_ < NOW-ch_->rtt_avg_)
            Swap(new KeepAliveController(this)); // really, nothing to send
    } else { // FIXME: mandatory rescheduling after send/recv; based on state
        tint spacing = ch_->rtt_avg_ / cwnd_;
        if (ch_->data_out_.size() < cwnd_) { // have cwnd; not the right time yet
            Schedule(ch_->last_send_data_time_+spacing);
        } else {    // no free cwnd
            tint timeout = std::max( ch_->rtt_avg_+ch_->dev_avg_*4, 500*TINT_MSEC );
            assert(!ch_->data_out_.empty());
            Schedule(ch_->data_out_.front().time+timeout); // wait for ACK or timeout
        }
    }
}


void    CwndController::OnDataRecvd(bin64_t b) {
    if (b!=bin64_t::NONE && b!=bin64_t::ALL) {
        Schedule(NOW); // send ACK; todo: aggregate ACKs
    }
}
    
void    CwndController::OnAckRcvd(bin64_t ackd) {
    if (ackd==bin64_t::NONE) {
        dprintf("%s #%i sendctrl loss detected\n",tintstr(),ch_->id);
        if (NOW>last_change_+ch_->rtt_avg_) {
            cwnd_ /= 2;
            last_change_ = NOW;
        }
    } else {
        if (cwnd_<1)
            cwnd_ *= 2;
        else 
            cwnd_ += 1.0/cwnd_;
        dprintf("%s #%i sendctrl cwnd to %f\n",tintstr(),ch_->id,cwnd_);
    }
    tint spacing = ch_->rtt_avg_ / cwnd_;
    Schedule(ch_->last_send_time_+spacing);
}


void SlowStartController::OnAckRcvd (bin64_t pos) {
    if (pos!=bin64_t::NONE) {
        cwnd_ += 1;
        if (TINT_SEC*cwnd_/ch_->rtt_avg_>=10) {
            Schedule(NOW);
            Swap(new AIMDController(this,cwnd_));
        }
    } else 
        cwnd_ /= 2;
}
    
