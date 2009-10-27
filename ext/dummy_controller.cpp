/*
 *  dummy_controller.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/16/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include "p2tp.h"

using namespace p2tp;


/** Congestion window evolution: always 1. */
struct BasicController : public CongestionController {

    tbqueue         data_out_;
    tint            dev_avg, rtt_avg;
    tint            last_send_time, last_recv_time, last_cwnd_mark;
    int             cwnd, peer_cwnd, in_flight, cwnd_rcvd;
    
    BasicController (int chann_id) : CongestionController(chann_id),
    dev_avg(0), rtt_avg(TINT_SEC), 
    last_send_time(0), last_recv_time(0), last_cwnd_mark(0),
    cwnd(1), peer_cwnd(1), in_flight(0), cwnd_rcvd(0)
    { }
    
    tint    RoundTripTime() {
        return rtt_avg;
    }
    
    tint    RoundTripTimeoutTime() {
        return rtt_avg + dev_avg * 8 + TINT_MSEC;
    }
    
    int     PeerBPS() {
        return (peer_cwnd<<10) * TINT_SEC / rtt_avg;
    }
    
    float PeerCWindow() {
        return peer_cwnd;
    }

    /** It is provided with an argument when sending time is not clear from the context. */
    tint    get_send_time () {
        tint time = TINT_NEVER;
        if (cwnd) {
            // cwnd allows => schedule transmit
            // otherwise => schedule timeout; may schedule transmit later
            if (free_cwnd())
                time = last_send_time + RoundTripTime()/cwnd; // next send
            else
                time = last_send_time + RoundTripTimeoutTime(); // timeout
        } else {
            time = last_send_time + TINT_SEC*58;
        }
        return time;
    }
    
    int     free_cwnd () {
        tint timeout = Datagram::now - RoundTripTimeoutTime();
        if (!data_out_.empty() && data_out_.front().time<=timeout) {
            data_out_.clear();
            cwnd >>= 1;
            if (!cwnd)
                cwnd = 1;
            dprintf("%s #%i loss cwnd:=%i\n",Datagram::TimeStr(),channel_id,cwnd);
        }
        return cwnd - data_out_.size();
    }
    
    tint OnDataSent(bin64_t b) {
        last_send_time = Datagram::now;
        if (b==bin64_t::ALL) { // nothing to send, absolutely
            data_out_.clear();
            cwnd = 0;
        } else if (b==bin64_t::NONE) { // sent some metadata
            cwnd = 1; // no more data => no need for cwnd
            data_out_.push_back(b);
        } else {
            data_out_.push_back(b);
        }
        dprintf("%s #%i cwnd %i infl %i peer_cwnd %i\n",
                Datagram::TimeStr(),channel_id,cwnd,in_flight,peer_cwnd);
        return get_send_time();
    }
    
    /*tint OnHintRecvd (bin64_t hint) {
        if (!cwnd) {
            cwnd = 1;
        }
        return get_send_time();
    }*/

    tint OnDataRecvd(bin64_t b) {
        if (data_out_.size() && data_out_.front().bin==bin64_t::NONE)
            data_out_.pop_front();
        if (b==bin64_t::NONE) {  // pong
            peer_cwnd = 1;
            if (!cwnd)
                cwnd = 1;
            return Datagram::now;
        } else if (b==bin64_t::ALL) { // the peer has nothing to send
            //peer_cwnd = 0;
            return get_send_time();
        } else {
            //if (!peer_cwnd)
            //    peer_cwnd = 1;
            cwnd_rcvd++;
            if (last_cwnd_mark+rtt_avg<Datagram::now) {
                last_cwnd_mark = Datagram::now;
                peer_cwnd = cwnd_rcvd;
                cwnd_rcvd = 0;
            }
            return Datagram::now; // at least, send an ACK
        }
    }
    
    tint OnAckRcvd(bin64_t ackd, tint when) {
        tbqueue tmp;
        for (int i=0; data_out_.size() && i<6; i++) {
            tintbin x = data_out_.front();
            data_out_.pop_front();
            if (x.bin==ackd) {
                // van Jacobson's rtt
                tint rtt = Datagram::now-x.time;
                if (rtt_avg==TINT_SEC) {
                    rtt_avg = rtt;
                } else {
                    rtt_avg = (rtt_avg*3 + rtt) >> 2;
                    dev_avg = ( dev_avg*3 + abs(rtt-rtt_avg) ) >> 2;
                }
                dprintf("%s #%i rtt %lli dev %lli\n",
                        Datagram::TimeStr(),channel_id,rtt_avg,dev_avg);
                // insert AIMD (2) here
                break;
            } else {
                tmp.push_back(x);
            }
        }
        while (tmp.size()) {
            data_out_.push_front(tmp.back());
            tmp.pop_back();
        }

        return get_send_time();
    }
   
    ~BasicController() {
    }
    
};


/*
 
 /** A packet was sent; in case it had data, b is the bin. *
void OnDataSent(bin64_t b) {
    if (b==bin64_t::NONE) {
        if (free_cwnd()>0) { // nothing to send; suspend
            cwnd = 0; 
            in_flight = 0;  
            set_send_time(last_send_time+KEEPALIVE);
        } else if (cwnd==0) { // suspended; keepalives only
            set_send_time(last_send_time+KEEPALIVE);
        } else { // probably, packet loss => stall
            tint timeout = last_send_time + rtt_avg + (dev_avg<<2);
            if (timeout<=Datagram::now) { // loss
                if (timeout+2*rtt_avg>Datagram::now) {
                    in_flight = 0;
                    set_send_time(Datagram::now);
                } else { // too bad
                    set_send_time(TINT_NEVER);
                }
            } else
                set_send_time(timeout);
        }
    } else { // HANDSHAKE goes here with b==ALL
        in_flight++;
        set_send_time(Datagram::now + rtt_avg/cwnd);
    }
    last_bin_sent = b;
    last_send_time = Datagram::now;
}

void OnDataRecvd(bin64_t b) {
    last_recv_time = Datagram::now;
    set_send_time(Datagram::now); // to send a reply if needed
}

void OnAckRcvd(const tintbin& ack) {
    last_recv_time = Datagram::now;
    if (ack==bin64_t::NONE || last_bin_sent!=ack.bin)
        return;
    last_bin_sent = bin64_t::NONE;
    in_flight--;
    tint nst = last_send_time + ( cwnd  ?  rtt_avg/cwnd : KEEPALIVE );
    if (nst<Datagram::now) 
        nst = Datagram::now; // in case we were waiting for free cwnd space
    set_send_time(nst);
} // TODO: dont distinguish last send time and last data sent time
// SOLUTION: once free_cwnd==0 => don't invoke OnDataSent
// TODO: once it's time, but free_cwnd=0 => need to set timeout

 */