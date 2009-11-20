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
    tbqueue         hint_out_;
    FileTransfer*   transfer_;
    uint64_t        twist_;
    
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
        while (hint_out_.size() && hint_out_.front().time<NOW-TINT_SEC*3/2) {
            ack_hint_out_.copy_range(file().ack_out(), hint_out_.front().bin);
            hint_out_.pop_front();
        }
        if (!file().size()) {
            return bin64_t(0,0); // whoever sends it first
        }
    retry:
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
        if (hint==bin64_t::NONE) {
            return hint; // TODO: end-game mode
        }
        if (!file().ack_out().is_empty(hint)) { // unhinted/late data
            ack_hint_out_.copy_range(file().ack_out(), hint);
            goto retry;
        }
        while (hint.width()>max_width)
            hint = hint.left();
        assert(ack_hint_out_.get(hint)==bins::EMPTY);
        ack_hint_out_.set(hint);
        hint_out_.push_back(tintbin(NOW,hint));
        return hint;
    }
    
};
