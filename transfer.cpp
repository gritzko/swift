/*
 *  transfer.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifdef _WIN32
#include "compat.h"
#else
#include <sys/mman.h>
#endif
#include <errno.h>
#include <string>
#include <sstream>
#include "p2tp.h"
#include "compat/util.h"

#include "ext/seq_picker.cpp" // FIXME FIXME FIXME FIXME 

using namespace p2tp;

std::vector<FileTransfer*> FileTransfer::files(20);

#define BINHASHSIZE (sizeof(bin64_t)+sizeof(Sha1Hash))

// FIXME: separate Bootstrap() and Download(), then Size(), Progress(), SeqProgress()

FileTransfer::FileTransfer (const char* filename, const Sha1Hash& _root_hash) :
    file_(filename,_root_hash), hs_in_offset_(0)
{
    if (files.size()<fd()+1)
        files.resize(fd()+1);
    files[fd()] = this;
    picker_ = new SeqPiecePicker(this);
    picker_->Randomize(rand()&63);
    init_time_ = Datagram::Time();
}



FileTransfer::~FileTransfer ()
{

    files[fd()] = NULL;
    for(int i=0; i<Channel::channels.size(); i++) 
        if (Channel::channels[i] && Channel::channels[i]->transfer_==this) 
            delete Channel::channels[i];
}


FileTransfer* FileTransfer::Find (const Sha1Hash& root_hash) {
    for(int i=0; i<files.size(); i++)
        if (files[i] && files[i]->root_hash()==root_hash)
            return files[i];
    return NULL;
}


void            FileTransfer::OnPexIn (const Address& addr) {
    for(int i=0; i<hs_in_.size(); i++) {
        Channel* c = Channel::channels[hs_in_[i]];
        if (c && c->transfer().fd()==this->fd() && c->peer_==addr)
            return; // already connected
    }
    if (hs_in_.size()<20) {
        new Channel(this,Channel::sockets[0],addr);
    } else {
        pex_in_.push_back(addr);
        if (pex_in_.size()>1000)
            pex_in_.pop_front();
    }
}


int        FileTransfer::RevealChannel (int& pex_out_) {
    pex_out_ -= hs_in_offset_;
    if (pex_out_<0)
        pex_out_ = 0;
    while (pex_out_<hs_in_.size()) {
        Channel* c = Channel::channels[hs_in_[pex_out_]];
        if (c && c->transfer().fd()==this->fd()) {
            pex_out_ += hs_in_offset_ + 1;
            return c->id;
        } else {
            hs_in_[pex_out_] = hs_in_[0];
            hs_in_.pop_front();
            hs_in_offset_++;
        }
    }
    pex_out_ += hs_in_offset_;
    return -1;
}

