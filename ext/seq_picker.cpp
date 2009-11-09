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
    
    virtual bin64_t Pick (bins& offer, uint8_t layer) {
        //dprintf("twist is %lli\n",twist_);
        if (!file().size())
            return bin64_t(0,0); // a hack to get peak hashes; FIXME
        twist_ &= (file().peak(0)) & ((1<<6)-1);
        if (twist_) {
            offer.twist(twist_);
            ack_hint_out_.twist(twist_);
        }
        bin64_t hint = offer.find_filtered
                (ack_hint_out_,bin64_t::ALL,layer,bins::FILLED);
        if (twist_) {
            hint = hint.twisted(twist_);
            offer.twist(0);
            ack_hint_out_.twist(0);
        }
        if (hint==bin64_t::NONE)
            return hint; // TODO: end-game mode
        while (hint.layer()>layer)
            hint = hint.left();
        ack_hint_out_.set(hint);
        return hint;
        /*for (int l=layer; l>=0; l--) {
            for(int i=0; i<file_->peak_count(); i++) {
                bin64_t pick = may_pick.find(file_->peak(i),l,bins::FILLED);
                if (pick!=bin64_t::NONE)
                    return pick;
            }
        }
        return bin64_t::NONE;*/
    }
    
    virtual void    Received (bin64_t b) {
        ack_hint_out_.set(b,bins::FILLED);
    }
    
    virtual void    Expired (bin64_t b) {
        ack_hint_out_.copy_range(file().ack_out(),b);
    }
    
};
