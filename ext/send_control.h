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
    
    virtual const char* type() const = 0;
    
    virtual bool    MaySendData() = 0;
    virtual tint    NextSendTime () = 0;
    
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
    
    int     fails_;

    PingPongController (SendController* orig) : SendController(orig), fails_(0) {} 
    PingPongController (Channel* ch) : fails_(0), SendController(ch) {}
    const char* type() const { return "PingPong"; }
    bool    MaySendData();
    tint    NextSendTime ();
    void    OnDataSent(bin64_t b);
    void    OnDataRecvd(bin64_t b);
    void    OnAckRcvd(bin64_t ackd) ;
    ~PingPongController() {}
    
};


struct KeepAliveController : public SendController {

    tint delay_;
    
    KeepAliveController(SendController* prev) : SendController(prev),
    delay_(0) {}
    const char* type() const { return "KeepAlive"; }
    bool    MaySendData();
    tint    NextSendTime () ;
    void    OnDataSent(bin64_t b) ;
    void    OnDataRecvd(bin64_t b) ;
    void    OnAckRcvd(bin64_t ackd) ;
    
};


struct CwndController : public SendController {
    
    double   cwnd_;
    
    CwndController(SendController* orig, int cwnd=1) :
    SendController(orig), cwnd_(cwnd) {    }
    
    bool    MaySendData() ;
    tint    NextSendTime () ;
    void    OnDataSent(bin64_t b) ;
    void    OnDataRecvd(bin64_t b) ;
    void    OnAckRcvd(bin64_t ackd) ;
    
};


struct SlowStartController : public CwndController {
    
    SlowStartController(SendController* orig, int cwnd=1) : CwndController(orig,cwnd) {}
    const char* type() const { return "SlowStart"; }
    void    OnAckRcvd(bin64_t ackd) ;
    
};


struct AIMDController : public CwndController {
    
    AIMDController(SendController* orig, int cwnd=1) : CwndController(orig,cwnd) {}
    const char* type() const { return "AIMD"; }
    void    OnAckRcvd(bin64_t ackd) ;
    
};


#endif
