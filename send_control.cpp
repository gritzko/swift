/*
 *  send_control.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 12/10/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include "p2tp.h"

using namespace p2tp;
using namespace std;

tint Channel::MIN_DEV = 50*TINT_MSEC;
tint Channel::MAX_SEND_INTERVAL = TINT_SEC*58;
tint Channel::LEDBAT_TARGET = TINT_MSEC*25;
float Channel::LEDBAT_GAIN = 1.0/LEDBAT_TARGET;
tint Channel::LEDBAT_DELAY_BIN = TINT_SEC*30;


tint    Channel::NextSendTime () {
    switch (send_control_) {
        case KEEP_ALIVE_CONTROL: return KeepAliveNextSendTime();
        case PING_PONG_CONTROL:  return PingPongNextSendTime();
        case SLOW_START_CONTROL: return SlowStartNextSendTime();
        case AIMD_CONTROL:       return AimdNextSendTime();
        case LEDBAT_CONTROL:     return LedbatNextSendTime();
        default:                 assert(false);
    }
}

tint    Channel::SwitchSendControl (int control_mode) {
    dprintf("%s #%u sendctrl %i->%i\n",tintstr(),id,send_control_,control_mode);
    switch (control_mode) {
        case KEEP_ALIVE_CONTROL:
            send_interval_ = max(TINT_SEC/10,rtt_avg_);
            dev_avg_ = max(TINT_SEC,rtt_avg_);
            cwnd_ = 1;
            break;
        case PING_PONG_CONTROL:
            dev_avg_ = max(TINT_SEC,rtt_avg_);
            cwnd_ = 1;
            break;
        case SLOW_START_CONTROL:
            break;
        case AIMD_CONTROL:
            break;
        case LEDBAT_CONTROL:
            break;
        default: 
            assert(false);
    }
    send_control_ = control_mode;
    return NextSendTime();
}

// TODO: transitions, consistently
// TODO: may send data
tint    Channel::KeepAliveNextSendTime () {
    if (sent_since_recv_>=3 && last_recv_time_<NOW-TINT_MIN)
        return TINT_NEVER;
    if (ack_rcvd_recent_)
        return SwitchSendControl(SLOW_START_CONTROL);
    send_interval_ <<= 1;
    if (send_interval_>MAX_SEND_INTERVAL)
        send_interval_ = MAX_SEND_INTERVAL;
    return last_send_time_ + send_interval_;
}

tint    Channel::PingPongNextSendTime () {
    if (last_recv_time_ < last_send_time_-TINT_SEC*3) {
        // FIXME keepalive <-> pingpong (peers, transition)
    } // last_data_out_time_ < last_send_time_ - TINT_SEC...
    if (false)
        return SwitchSendControl(KEEP_ALIVE_CONTROL);
    if (ack_rcvd_recent_)
        return SwitchSendControl(SLOW_START_CONTROL);
    if (last_recv_time_>last_send_time_)
        return NOW;
    else if (last_send_time_)
        return last_send_time_ + ack_timeout();
    else
        return NOW;
}

tint    Channel::CwndRateNextSendTime () {
    send_interval_ = rtt_avg_/cwnd_;
    if (data_out_.size()<cwnd_) {
	dprintf("%s #%u sendctrl next in %llius\n",tintstr(),id,send_interval_);
        return last_data_out_time_ + send_interval_;
    } else {
        tint next_timeout = data_out_.front().time + ack_timeout();
        return last_data_out_time_ + next_timeout;
    }
}

void    Channel::BackOffOnLosses () {
    ack_rcvd_recent_ = 0;
    ack_not_rcvd_recent_ =  0;
    if (last_loss_time_<NOW-rtt_avg_) {
        cwnd_ /= 2;
        last_loss_time_ = NOW;
	dprintf("%s #%u sendctrl backoff %3.2f\n",tintstr(),id,cwnd_);
    }
}

tint    Channel::SlowStartNextSendTime () {
    if (ack_not_rcvd_recent_) {
        BackOffOnLosses();
        return SwitchSendControl(AIMD_CONTROL);
    } 
    if (send_interval_<TINT_SEC/10)
        return SwitchSendControl(AIMD_CONTROL);
    cwnd_+=ack_rcvd_recent_;
    ack_rcvd_recent_=0;
    return CwndRateNextSendTime();
}

tint    Channel::AimdNextSendTime () {
    if (ack_not_rcvd_recent_)
        BackOffOnLosses();
    cwnd_ += ack_rcvd_recent_/cwnd_;
    ack_rcvd_recent_=0;
    return CwndRateNextSendTime();
}

tint Channel::LedbatNextSendTime () {
    tint owd_cur(TINT_NEVER), owd_min(TINT_NEVER);
    for(int i=0; i<4; i++) {
        if (owd_min>owd_min_bins_[i])
            owd_min = owd_min_bins_[i];
        if (owd_cur>owd_current_[i])
            owd_cur = owd_current_[i];
    }
    if (ack_not_rcvd_recent_)
        BackOffOnLosses();
    ack_rcvd_recent_ = 0;
    tint queueing_delay = owd_cur - owd_min;
    tint off_target = LEDBAT_TARGET - queueing_delay;
    cwnd_ += LEDBAT_GAIN * off_target / cwnd_;
    return CwndRateNextSendTime();
}



