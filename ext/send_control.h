/*
 *  send_control.h
 *  p2tp
 *
 *  Created by Victor Grishchenko on 11/4/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
// included into p2tp.h
#ifndef P2TP_SEND_CONTROL
#define P2TP_SEND_CONTROL

class Channel;

struct SendController {
    
    Channel*    ch_;
    
    SendController (Channel* ch) : ch_(ch) {}
    
    SendController(SendController* orig) : ch_(orig->ch_) { }
    
    void    Swap (SendController* replacement);
    void    Schedule (tint time);
    
    virtual const char* type() const = 0;
    
    virtual bool    MaySendData() = 0;
    
    /** A datagram was sent to the peer.
     *  @param data the bin number for the data sent; bin64_t::NONE if only
                    metadata was sent; bin64_t::ALL if datagram was empty */
    virtual void    OnDataSent(bin64_t data) = 0;
    
    /** A datagram was received from the peer.
        @param data follows the same conventions as data in OnDataSent() */
    virtual void    OnDataRecvd(bin64_t data) = 0;
    
    /** An acknowledgement on OUR data message was receiveed from the peer.
        @param ackd bin number for the data sent; bin64_t::NONE if no
                    acknowledgement was received (timeout event) */
    virtual void    OnAckRcvd(bin64_t ackd) = 0;
    
    virtual         ~SendController() {}
};

struct PingPongController : public SendController {
    
    int     sent_, unanswered_;
    
    PingPongController (SendController* orig) :
        SendController(orig), sent_(0), unanswered_(0) {} 
    PingPongController (Channel* ch) : 
        unanswered_(0), sent_(0), SendController(ch) {}
    const char* type() const { return "PingPong"; }
    bool    MaySendData();
    void    OnDataSent(bin64_t b);
    void    OnDataRecvd(bin64_t b);
    void    OnAckRcvd(bin64_t ackd) ;
    ~PingPongController() {}
    
};
/** Mission of the keepalive controller to keep the channel
    alive as no data sending happens; If no data is transmitted
    in either direction, inter-packet times grow exponentially
    till 58 sec, which refresh period is deemed necessary to keep
    NAT mappings alive. */
struct KeepAliveController : public SendController {

    tint delay_;
    
    KeepAliveController (Channel* ch);
    KeepAliveController(SendController* prev, tint delay=0) ;
    const char* type() const { return "KeepAlive"; }
    bool    MaySendData();
    void    OnDataSent(bin64_t b) ;
    void    OnDataRecvd(bin64_t b) ;
    void    OnAckRcvd(bin64_t ackd) ;
    
};


/** Base class for any congestion window based algorithm. */
struct CwndController : public SendController {
    
    double  cwnd_;
    tint    last_change_;
    
    CwndController(SendController* orig, int cwnd=1) ;
    
    bool    MaySendData() ;
    void    OnDataSent(bin64_t b) ;
    void    OnDataRecvd(bin64_t b) ;
    void    OnAckRcvd(bin64_t ackd) ;
    
};


/** TCP-like exponential "slow" start algorithm. */
struct SlowStartController : public CwndController {
    
    SlowStartController(SendController* orig, int cwnd=1) : CwndController(orig,cwnd) {}
    const char* type() const { return "SlowStart"; }
    void    OnAckRcvd(bin64_t ackd) ;
    
};


/** The classics: additive increase - multiplicative decrease algorithm.
    A naive version of "standard" TCP congestion control. Potentially useful
    for seedboxes, so needs to be improved. (QUBIC?) */
struct AIMDController : public CwndController {
    
    AIMDController(SendController* orig, int cwnd=1) : CwndController(orig,cwnd) {}
    const char* type() const { return "AIMD"; }
    //void    OnAckRcvd(bin64_t ackd) ;
    
};


#endif
