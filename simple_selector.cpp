/*
 *  simple_selector.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */

#include "p2tp.h"

using namespace p2tp;

class SimpleSelector : public PeerSelector {
    typedef std::pair<sockaddr_in,uint32_t> memo_t;
    std::queue<memo_t>  peers;
public:
    virtual void PeerKnown (const Sha1Hash& root, struct sockaddr_in& addr) {
        peers.push_front(memo_t(addr,root.fingerprint()));
    }
    virtual sockaddr_in GetPeer (const Sha1Hash& for_root) {
        uint32_t fp = for_root.fingerprint();
        for(std::queue<memo_t>::iterator i=peers.begin(); i!=peers.end(); i++)
            if (i->second==fp) {
                i->second = 0;
                sockaddr_in ret = i->first;
                while (peers.begin()->second==0)
                    peers.pop_front();
                return ret;
            }
    }
};

static Channel::peer_selector = new SimpleSelector();