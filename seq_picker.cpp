/*
 *  seq_picker.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */

#include "p2tp.h"

using namespace p2tp;

class SeqPiecePicker : public PiecePicker {
    
    bins hint_out;
    FileTransfer* file;
    
public:
    
    SeqPiecePicker (FileTransfer* file_) : file(file_) {
        diho(file->ack_out);
    }
    
    virtual bin64_t Pick (bins& from, uint8_t layer) {
        bins may_pick = ~ file->ack_out;
        may_pick &= from;
        may_pick -= hint_out;
        bin64_t pick = may_pick.find(file->top,bins::FILLED);
        if ( pick==bin64_t::NONE || pick.right_foot() > file->size() )
            if (layer)
                return Pick(from,layer-1);
            else
                return bin64_t::NONE;
        return pick;
    }
    
    virtual void    Received (bin64_t b) {
        diho.set(b,bins::FILLED);
    }
    
    virtual void    Snubbed (bin64_t b) {
        diho.set(b,bins::EMPTY);
    }
    
};