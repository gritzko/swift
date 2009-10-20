/*
 *  simple_selector.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include <queue>
#include "p2tp.h"

using namespace p2tp;

class SimpleSelector : public PeerSelector {
    typedef std::pair<Address,Sha1Hash> memo_t;
    typedef std::deque<memo_t>  peer_queue_t;
    peer_queue_t    peers;
public:
    SimpleSelector () {
    }
    void AddPeer (const Datagram::Address& addr, const Sha1Hash& root) {
        peers.push_front(memo_t(addr,root)); //,root.fingerprint() !!!
    }
    Address GetPeer (const Sha1Hash& for_root) {
        //uint32_t fp = for_root.fingerprint();
        for(peer_queue_t::iterator i=peers.begin(); i!=peers.end(); i++)
            if (i->second==for_root) {
                i->second = 0;
                sockaddr_in ret = i->first;
                while (peers.begin()->second==0)
                    peers.pop_front();
                return ret;
            }
        return Address();
    }
};

PeerSelector* Channel::peer_selector = new SimpleSelector();