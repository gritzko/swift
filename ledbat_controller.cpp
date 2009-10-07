/*
 *  ledbat_controller.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */

#include "p2tp.h"

using namespace p2tp;

class LedbatController : public CongestionController {
public:
    /*tint    rtt_avg;
    tint    dev_avg;
    int     cwnd;
    int     peer_cwnd;*/
    virtual void OnTimeout () {
    }
    
    virtual void OnDataSent(bin64_t b) {
    }
    
    virtual void OnDataRecvd(bin64_t b) {
    }
    
    virtual void OnAckRcvd(bin64_t b, tint peer_stamp) {
    }
    
    virtual ~CongestionControl() {
    }
    
};
