/*
 *  transfer.cpp
 *  some transfer-scope code
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include <errno.h>
#include <string>
#include <sstream>
#include "swift.h"

#include "ext/seq_picker.cpp" // FIXME FIXME FIXME FIXME 

using namespace swift;

std::vector<FileTransfer*> FileTransfer::files(20);

#define BINHASHSIZE (sizeof(bin64_t)+sizeof(Sha1Hash))

// FIXME: separate Bootstrap() and Download(), then Size(), Progress(), SeqProgress()

FileTransfer::FileTransfer (const char* filename, const Sha1Hash& _root_hash) :
    file_(filename,_root_hash), hs_in_offset_(0), cb_installed(0)
{
    if (files.size()<fd()+1)
        files.resize(fd()+1);
    files[fd()] = this;
    picker_ = new SeqPiecePicker(this);
    picker_->Randomize(rand()&63);
    init_time_ = Datagram::Time();
}


void    Channel::CloseTransfer (FileTransfer* trans) {
    for(int i=0; i<Channel::channels.size(); i++) 
        if (Channel::channels[i] && Channel::channels[i]->transfer_==trans) 
            delete Channel::channels[i];
}


void swift::AddProgressCallback (int transfer,ProgressCallback cb,uint8_t agg) {
    FileTransfer* trans = FileTransfer::file(transfer);
    if (!trans)
        return;
    trans->cb_agg[trans->cb_installed] = agg;
    trans->callbacks[trans->cb_installed++] = cb;
}


void swift::ExternallyRetrieved (int transfer,bin64_t piece) {
    FileTransfer* trans = FileTransfer::file(transfer);
    if (!trans)
        return;
    trans->ack_out().set(piece); // that easy
}


void swift::RemoveProgressCallback (int transfer, ProgressCallback cb) {
    FileTransfer* trans = FileTransfer::file(transfer);
    if (!trans)
        return;
    for(int i=0; i<trans->cb_installed; i++)
        if (trans->callbacks[i]==cb)
            trans->callbacks[i]=trans->callbacks[--trans->cb_installed];
}


FileTransfer::~FileTransfer ()
{
    Channel::CloseTransfer(this);
    files[fd()] = NULL;
    delete picker_;
}


FileTransfer* FileTransfer::Find (const Sha1Hash& root_hash) {
    for(int i=0; i<files.size(); i++)
        if (files[i] && files[i]->root_hash()==root_hash)
            return files[i];
    return NULL;
}


int       swift:: Find (Sha1Hash hash) {
    FileTransfer* t = FileTransfer::Find(hash);
    if (t)
        return t->fd();
    return -1;
}



void            FileTransfer::OnPexIn (const Address& addr) {
    for(int i=0; i<hs_in_.size(); i++) {
        Channel* c = Channel::channel(hs_in_[i]);
        if (c && c->transfer().fd()==this->fd() && c->peer()==addr)
            return; // already connected
    }
    if (hs_in_.size()<20) {
        new Channel(this,Datagram::default_socket(),addr);
    } else {
        pex_in_.push_back(addr);
        if (pex_in_.size()>1000)
            pex_in_.pop_front();
    }
}


int        FileTransfer::RevealChannel (int& pex_out_) { // FIXME brainfuck
    pex_out_ -= hs_in_offset_;
    if (pex_out_<0)
        pex_out_ = 0;
    while (pex_out_<hs_in_.size()) {
        Channel* c = Channel::channel(hs_in_[pex_out_]);
        if (c && c->transfer().fd()==this->fd()) {
            if (c->is_established()) {
                pex_out_ += hs_in_offset_ + 1;
                return c->id();
            } else
                pex_out_++;
        } else {
            hs_in_[pex_out_] = hs_in_[0];
            hs_in_.pop_front();
            hs_in_offset_++;
        }
    }
    pex_out_ += hs_in_offset_;
    return -1;
}

