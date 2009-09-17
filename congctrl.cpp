/*
 *  congctrl.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 4/14/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include "p2tp.h"
#include <glog/logging.h>

using namespace p2tp;

CongestionControl::CongestionControl () {
	state_ = SLOW_START_STATE;
	cwnd_ = 1;
	cainc_ = 0;
	ssthresh_ = 100;
	rtt_avg_ = 0;
	dev_avg_ = 0;
	peer_cwnd_ = 0;
	data_ins_ = 0;
	last_arrival_ = 0;
	rate_ = TINT_1SEC/10;
}


void CongestionControl::RttSample (tint rtt) {
    if (rtt_avg_>0) {
        rtt_avg_ = (rtt_avg_*7 + rtt) >> 3; // affected by reordering
        dev_avg_ = (dev_avg_*7 + ::abs(rtt-rtt_avg_)) >> 3;
    } else {
        rtt_avg_ = rtt;
        dev_avg_ = rtt>>3;
    }
    DLOG(INFO)<<"sample "<<rtt<<" rtt "<<rtt_avg_<<" dev "<<dev_avg_;
}


void	CongestionControl::OnCongestionEvent (CongCtrlEvents ev) {
	switch (ev) {
		case LOSS_EV:	
			cwnd_ /= 2;
			state_ = CONG_AVOID_STATE;
			break;
		case ACK_EV:
			if (state_==SLOW_START_STATE) {
				cwnd_++;
				if (cwnd_>=ssthresh_)
					state_ = CONG_AVOID_STATE;
			} else if (state_==CONG_AVOID_STATE) {
				cainc_++;
				if (cainc_>=cwnd_) {
					cainc_ = 0;
					cwnd_++;
				}
			}
			break;
		case DATA_EV:
			tint interarrival = last_arrival_ ? 
				Datagram::now - last_arrival_	:
				rtt_avg_;	// starting est. corresp. cwnd==1
			last_arrival_ = Datagram::now;
			if (rate_)
				rate_ = ( rate_*3 + interarrival ) / 4;
			else
				rate_ = interarrival;
			break;
	}
	DLOG(INFO)<<"peer irr "<<rate_<<" pcwnd "<<peer_cwnd();
}

int		CongestionControl::peer_cwnd () const {
	if (!rate_)
		return 0;
	int pc = rtt_avg_ / rate_;
	if ( rtt_avg_%rate_ > rate_/2 ) // too many /
		pc++;
	return pc;
}

int		CongestionControl::peer_bps () const {
	return 1024 * TINT_1SEC / rate_;
}
