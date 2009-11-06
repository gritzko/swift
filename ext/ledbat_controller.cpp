/*
 *  ledbat_controller.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include "p2tp.h"

using namespace p2tp;

class LedbatController : public CongestionController {
public:
    tint dev_avg_, rtt_avg_;
    tint last_send_time_, last_recv_time_;
    int cwnd_, peer_cwnd_, in_flight_;
    bin64_t last_bin_sent_;
public:
    LedbatController (int chid) : dev_avg_(0), rtt_avg_(TINT_SEC), last_send_time_(0),
    last_recv_time_(0), cwnd_(1), peer_cwnd_(1), in_flight_(0), CongestionController(chid) {
    }
    
    tint    rtt_avg () {
        return rtt_avg_;
    }
    
    tint    dev_avg () {
        return dev_avg_;
    }
    
    int     cwnd () {
        // check for timeouts
        return cwnd_;
    }
    
    int     peer_cwnd () {
        return peer_cwnd_;
    }
    
    int     free_cwnd ( ){
        return cwnd() - in_flight_;
    }
    
    tint    next_send_time ( ){
        return cwnd_ ? NOW + (rtt_avg_/cwnd_) : TINT_NEVER; // TODO keepalives
    }
    
    void OnDataSent(bin64_t b) {
        if (b==bin64_t::NONE) {
            cwnd_ = 0; // nothing to send; suspend
        } else {
            last_bin_sent_ = b;
            last_send_time_ = NOW;
            in_flight_++;
        }
    }
    
    void OnDataRecvd(bin64_t b) {
        last_recv_time_ = NOW;
    }
    
    void OnAckRcvd(bin64_t b, tint peer_stamp) {
        if (last_bin_sent_!=b)
            return;
        if (peer_stamp!=TINT_NEVER)
            rtt_avg_ = (rtt_avg_*7 + (NOW-last_send_time_)) >> 3; // van Jac
        in_flight_--;
    }
    
    ~LedbatController() {
    }
};
