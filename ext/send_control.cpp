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
    dprintf("%s #%i sendctrl %s->%s\n",Datagram::TimeStr(),ch_->id,type(),newctrl->type());
    assert(this==ch_->cc_);
    ch_->cc_ = newctrl;
    delete this;
}


bool    PingPongController::MaySendData(){
    return ch_->data_out_.empty();
}
    
tint    PingPongController::NextSendTime () {
    return ch_->last_send_time_ + ch_->rtt_avg_ + ch_->dev_avg_*4;  // remind on timeout
}
    
void    PingPongController::OnDataSent(bin64_t b) {
    if ( (ch_->last_recv_time_ && ch_->last_recv_time_<Datagram::now-TINT_SEC*3) || //no reply
         (b==bin64_t::ALL && MaySendData()) ) // nothing to send
        Swap(new KeepAliveController(this));
}
    
void    PingPongController::OnDataRecvd(bin64_t b) {
}
    
void    PingPongController::OnAckRcvd(bin64_t ackd) {
    //if (ch_->data_out_.empty())
    Swap(new SlowStartController(this));
}


    
bool    KeepAliveController::MaySendData() {
    return true;
}
    
tint    KeepAliveController::NextSendTime () {
    return ch_->last_send_time_ + TINT_SEC*58;
}
    
void    KeepAliveController::OnDataSent(bin64_t b) {
    if (b!=bin64_t::ALL)
        Swap(new PingPongController(this));
}
    
void    KeepAliveController::OnDataRecvd(bin64_t b) {
}
    
void    KeepAliveController::OnAckRcvd(bin64_t ackd) {
}
    


bool    CwndController::MaySendData() {
    dprintf("%s #%i maysend %i < %f & %s (rtt %lli)\n",Datagram::TimeStr(),
            ch_->id,(int)ch_->data_out_.size(),cwnd_,Datagram::TimeStr(NextSendTime()),
            ch_->rtt_avg_);
    return ch_->data_out_.size() < cwnd_  &&  Datagram::now >= NextSendTime();
}
    
tint    CwndController::NextSendTime () {
    tint sendtime;
    if (ch_->data_out_.size() < cwnd_)
        sendtime = ch_->last_send_time_ + (ch_->rtt_avg_ / cwnd_);
    else
        sendtime = ch_->last_send_time_ + ch_->rtt_avg_ + ch_->dev_avg_ * 4 ;
    return sendtime;
}
    
void    CwndController::OnDataSent(bin64_t b) {
    if (b==bin64_t::ALL || b==bin64_t::NONE) {
        if (MaySendData())
            Swap(new PingPongController(this));
    } 
}
    
void    CwndController::OnDataRecvd(bin64_t b) {
}
    
void    CwndController::OnAckRcvd(bin64_t ackd) {
    if (ackd==bin64_t::NONE) {
        cwnd_ /= 2;
    } else {
        if (cwnd_<1)
            cwnd_ *= 2;
        else 
            cwnd_ += 1/cwnd_;
    }
}


void SlowStartController::OnAckRcvd (bin64_t pos) {
    if (pos!=bin64_t::NONE) {
        cwnd_ += 1;
        if (TINT_SEC*cwnd_/ch_->rtt_avg_>=10)
            Swap(new AIMDController(this,cwnd_));
    } else 
        cwnd_ /= 2;
}
    

void AIMDController::OnAckRcvd (bin64_t pos) {
    if (pos!=bin64_t::NONE)
        cwnd_ += 1.0/cwnd_;
    else 
        cwnd_ /= 2;
}
 
