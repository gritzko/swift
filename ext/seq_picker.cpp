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
    
    bins hint_out_;
    FileTransfer* file_;
    
public:
    
    SeqPiecePicker (FileTransfer* file_to_pick_from) : file_(file_to_pick_from), hint_out_() {
    }
    
    virtual bin64_t Pick (bins& from, uint8_t layer) {
        bins may_pick = from;
        may_pick.remove (file_->ack_out());
        may_pick.remove (hint_out_);
        for (int l=layer; l>=0; l--) {
            for(int i=0; i<file_->peak_count(); i++) {
                bin64_t pick = may_pick.find(file_->peak(i),l,bins::FILLED);
                if (pick!=bin64_t::NONE)
                    return pick;
            }
        }
        return bin64_t::NONE;
    }
    
    virtual void    Received (bin64_t b) {
        hint_out_.set(b,bins::EMPTY);
    }
    
    virtual void    Snubbed (bin64_t b) {
        hint_out_.set(b,bins::EMPTY);
    }
    
};