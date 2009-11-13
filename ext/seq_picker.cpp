/*
 *  seq_picker.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include "p2tp.h"

using namespace p2tp;

class SeqPiecePicker : public PiecePicker {
    
    bins            ack_hint_out_;
    FileTransfer*   transfer_;
    uint64_t        twist_;
    tbheap          hint_out_; // FIXME since I use fixed 1.5 sec expiration, may replace for a queue
    
public:
    
    SeqPiecePicker (FileTransfer* file_to_pick_from) : 
    transfer_(file_to_pick_from), ack_hint_out_(), twist_(0) {
        ack_hint_out_.copy_range(file().ack_out(),bin64_t::ALL);
    }
    
    HashTree& file() { 
        return transfer_->file(); 
    }
    
    virtual void Randomize (uint64_t twist) {
        twist_ = twist;
    }
    
    virtual bin64_t Pick (bins& offer, uint64_t max_width, tint expires) {
        while (hint_out_.size() && hint_out_.peek().time<NOW)
            ack_hint_out_.copy_range(file().ack_out(), hint_out_.pop().bin);
        //dprintf("twist is %lli\n",twist_);
        if (!file().size()) {
            return bin64_t(0,0); // whoever sends it first
        }
        twist_ &= (file().peak(0)) & ((1<<6)-1);
        if (twist_) {
            offer.twist(twist_);
            ack_hint_out_.twist(twist_);
        }
        bin64_t hint = offer.find_filtered (ack_hint_out_,bin64_t::ALL,bins::FILLED);
        if (twist_) {
            hint = hint.twisted(twist_);
            offer.twist(0);
            ack_hint_out_.twist(0);
        }
        if (hint==bin64_t::NONE)
            return hint; // TODO: end-game mode
        while (hint.width()>max_width)
            hint = hint.left();
        assert(ack_hint_out_.get(hint)==bins::EMPTY);
        if (hint.offset() && file().ack_out().get(hint)!=bins::EMPTY) { // FIXME DEBUG remove
            eprintf("bogus hint: (%i,%lli)\n",(int)hint.layer(),hint.offset());
            exit(1);
        }
        ack_hint_out_.set(hint);
        hint_out_.push(tintbin(expires,hint));
        return hint;
    }
    
    void Received (bin64_t bin) {
        ack_hint_out_.set(bin);
    }
    
};
