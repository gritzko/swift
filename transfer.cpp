/*
 *  transfer.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */

#include "p2tp.h"


File::File (int _fd) : fd(_fd), status_(DONE), hashes(_fd)
{
	bin::vec peaks = bin::peaks(hashes.data_size());
	history.insert(history.end(),peaks.begin(),peaks.end());
	for(bin::vec::iterator i=peaks.begin(); i!=peaks.end(); i++)
		ack_out.set(*i);
}

File::File (Sha1Hash hash, int _fd) : hashes(hash), fd(_fd), status_(EMPTY) {
	// TODO resubmit data
}

File::~File() {
	if (fd>0) ::close(fd);
}


bool	File::OfferHash (bin pos, const Sha1Hash& hash) {
	HashTree::hashres_t res = hashes.offer(pos,hash);
	if (res==HashTree::PEAK_ACCEPT) { // file size is finally known
		ftruncate(fd, size());
		LOG(INFO)<<fd<<" file size is set to "<<size();
		history.push_back(0);
		status_ = IN_PROGRESS;
	}
	return res==HashTree::PEAK_ACCEPT || res==HashTree::ACCEPT;
}


File*	File::find (const Sha1Hash& hash) {
	for(vector<File*>::iterator i=files.begin(); i!=files.end(); i++)
		if (*i && (*i)->hashes.root==hash)
			return *i;
	return NULL;
}


int p2tp::Open (const char* filename) {
	int fd = ::open(filename,O_RDONLY);
	if (fd<0)
		return -1;
	if (File::files.size()<fd+1)
		File::files.resize(fd+1);
	File::files[fd] = new File(fd);
	return fd;
}

int p2tp::Open (const Sha1Hash& root_hash, const char* filename) {
	int fd = ::open(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd<0)
		return -1;
	if (File::files.size()<fd+1)
		File::files.resize(fd+1);
	File::files[fd] = new File(root_hash,fd);
	return fd;
}

size_t	p2tp::file_size (int fd) { return File::file(fd)->size(); }

void p2tp::Close (int fid) {
	if (!File::files[fid])
		return;
	delete File::files[fid];
	File::files[fid] = NULL;
}


Sha1Hash HashTree::deriveRoot () {
	int i = peak_count-1;
	bin64_t p = peaks[i].first;
	Sha1Hash hash = peak_hashes[i].second;
	i--;
	while (p<bin64_t::ALL) {
		if (p.is_left()) {
			p = p.parent();
			hash = Sha1Hash(hash,Sha1Hash::ZERO);
		} else {
			if (i<0 || peaks[i].first!=p.sibling())
				return Sha1Hash::ZERO;
			hash = Sha1Hash(peak_hashes[i].second,hash);
			p = p.parent();
			i--;
		}
	}
	return hash;
}

/** Three stages: have file, have root hash, have peaks. */
HashTree::HashTree (int _datafd, int _hashfd, Sha1Hash _root)
: datafd(_datafd), hashfd(_hashfd), root(_root) 
{
    if (root==Sha1Hash::ZERO) { // fresh file; derive the root hash
        struct stat st;
        if (fstat(fd, &st)!=0)
            return;
        resize(st.st_size);
        lseek(datafd,0,SEEK_SET);
        for(bin64_t k=0; k!=toppeak.right(); k=k.dfs_next()) {
            if (k.is_base()) {
                uint8_t data[1024];
                int rd = read(datafd,data,1024);
                if (rd<=0)
                    return; // FIXME
                hashes[k] = Sha1Hash(data,rd);
            } else
                hashes[k] = Sha1Hash(hashes[k.left()],hashes[k.right()]);
        }
        // zeros
        root = hashes[toppeak];
        for(bin64_t p=toppeak; p!=bin64_t::ALL; p=p.parent())
            root = Sha1Hash(root,Sha1Hash::ZERO);
    }
    // TODO:  THIS MUST BE THE "Transfer"/"File" CLASS
    if (file_size==0) { // hash only, no file, no peak hashes
        if (root==Sha1Hash::ZERO)
            return; // FIXME
        resize(0); // anyway, 1cell for the root, 63 for peaks
    }
    
}

bool FileTransfer::acceptData (uint64_t bin, uint8_t* data, size_t len) {
}


HashTree::~HashTree () {
	close(fd);
}

HashTree::hashres_t	HashTree::offerPeak (bin pos, Sha1Hash hash) {
	if (bin(pos+1).layer())
		return REJECT;
	if (bin::all1(pos))
		peaks.clear();
	peaks.push_back(binhash(pos,hash));
	if (deriveRoot()==root) { // bingo
		mass = peaks.back().first;
		length = mass.length();
		status.resize(mass+1);
		bits.resize(mass+1);
		for(int i=0; i<peaks.size(); i++) {
			bits[peaks[i].first] = peaks[i].second;
			status[peaks[i].first] = true;
		}
		return PEAK_ACCEPT;
	} else
		return pos.layer() ? DUNNO : REJECT;
}

HashTree::hashres_t	HashTree::offer (bin pos, const Sha1Hash& hash) {
	if (!length)  // only peak hashes are accepted at this point
		return offerPeak(pos,hash);
	if (pos>mass)
		return REJECT;
	if (status[pos])
		return bits[pos]==hash ? ACCEPT : REJECT;
	bits[pos] = hash;
	// walk to the nearest proven hash
	if (bits[pos.sibling()]==Sha1Hash::ZERO)
		return DUNNO;
	bin p = pos.parent();
	while (!status[p]) {
		bits[p] = Sha1Hash(bits[p.left()],bits[p.right()]);
		p = p.parent();
	}
	if ( bits[p] == Sha1Hash(bits[p.left()],bits[p.right()]) ) {
		for(bin i=pos; i<p; i=i.parent())
			status[i] = status[i.sibling()] = true;
		return ACCEPT;
	} else
		return REJECT;
	
}



