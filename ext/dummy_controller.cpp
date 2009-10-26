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

    bin64_t         last_bin_sent;
    tint            dev_avg, rtt_avg;
    tint            last_send_time, last_recv_time, last_cwnd_mark;
    int             cwnd, peer_cwnd, in_flight, cwnd_rcvd;
    
    BasicController () :
    dev_avg(0), rtt_avg(TINT_SEC), 
    last_send_time(0), last_recv_time(0), last_cwnd_mark(0),
    cwnd(1), peer_cwnd(1), in_flight(0), cwnd_rcvd(0)
    { }
    
    tint    RoundTripTime() {
        return rtt_avg;
    }
    
    tint    RoundTripTimeoutTime() {
        return rtt_avg + dev_avg * 4;
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
            if (in_flight<cwnd)
                time = last_send_time + rtt_avg/cwnd; // next send
            else
                time = last_send_time + rtt_avg + dev_avg*4; // timeout
        } else {
            time = last_send_time + TINT_SEC*58;
        }
        return time;
    }
    
    int     free_cwnd () {
        // check for timeout
        if (cwnd && cwnd==in_flight && Datagram::now>=last_send_time+RoundTripTimeoutTime()) {
            cwnd >>= 1;
            if (!cwnd)
                cwnd = 1;
            in_flight = 0;
            last_bin_sent = bin64_t::NONE; // clear history
        }
        return cwnd - in_flight;
    }
    
    tint OnDataSent(bin64_t b) {
        dprintf("%s 1 cwnd %i peer_cwnd %i\n",Datagram::TimeStr(),cwnd,peer_cwnd);
        last_send_time = Datagram::now;
        last_bin_sent = b;
        // sync the sending mode with the actual state of data to send
        if (b==bin64_t::NONE) { // sent some metadata
            cwnd = 1;
        } else if (b==bin64_t::ALL) {  // nothing to send, absolutely
            cwnd = 0;
        } else { // have data, use cong window sending
            cwnd = 1;  // TODO AIMD (1) here
            last_bin_sent = b;
            in_flight = 1;
        }
        dprintf("%s 2 cwnd %i peer_cwnd %i\n",Datagram::TimeStr(),cwnd,peer_cwnd);
        return get_send_time();
    }

    tint OnDataRecvd(bin64_t b) {
        if (b==bin64_t::NONE) {  // pong
            peer_cwnd = 1;
            return Datagram::now;
        } else if (b==bin64_t::ALL) { // the peer has nothing to send
            peer_cwnd = 0;
            if (cwnd==1) { // it was an implicit ACK, keep sending
                in_flight = 0;
                return Datagram::now;
            } else 
                return get_send_time();
        } else {
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
        if (ackd==last_bin_sent) { // calc cwnd/free
            // van Jacobson's rtt
            tint rtt = Datagram::now-last_send_time;
            rtt_avg = (rtt_avg*7 + rtt) >> 3;
            dev_avg = ( dev_avg*7 + abs(rtt-rtt_avg) ) >> 3;
            last_bin_sent = bin64_t::NONE;
            in_flight = 0;
            // insert AIMD (2) here
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