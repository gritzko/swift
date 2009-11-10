/*
 *  hashtree.h
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef P2TP_SHA1_HASH_TREE_H
#define P2TP_SHA1_HASH_TREE_H
#include "bin64.h"
#include "bins.h"
#include <string.h>
#include <string>

namespace p2tp {


struct Sha1Hash {
	uint8_t	bits[20];

	Sha1Hash() { memset(bits,0,20); }
	Sha1Hash(const Sha1Hash& left, const Sha1Hash& right);
	Sha1Hash(const char* str, size_t length=-1);
	Sha1Hash(const uint8_t* data, size_t length);
	Sha1Hash(bool hex, const char* hash);
	
	std::string	hex() const;
	bool	operator == (const Sha1Hash& b) const
		{ return 0==memcmp(bits,b.bits,SIZE); }
	bool	operator != (const Sha1Hash& b) const { return !(*this==b); }
    const char* operator * () const { return (char*) bits; }
	
	const static Sha1Hash ZERO;
	const static size_t SIZE;
};


class HashTree {

    /** Merkle hash tree: root */
	Sha1Hash        root_hash_;
    Sha1Hash        *hashes_;
    /** Merkle hash tree: peak hashes */
    Sha1Hash        peak_hashes_[64];
    bin64_t         peaks_[64];
    int             peak_count_;
    /** File descriptor to put hashes to */
	int             fd_;
    int             hash_fd_;
    /** Whether to re-hash files. */
    bool            data_recheck_;
    /** Base size, as derived from the hashes. */
    size_t          size_;
    size_t          sizek_;
    /**	Part of the tree currently checked. */
    size_t          complete_;
    size_t          completek_;
    bins            ack_out_;
    
protected:
    
    void            Submit();
    void            RecoverProgress();
    Sha1Hash        DeriveRoot();
    bool            OfferPeakHash (bin64_t pos, const Sha1Hash& hash);
    
public:
	
	HashTree (const char* file_name, const Sha1Hash& root=Sha1Hash::ZERO, 
              const char* hash_filename=NULL);
    
    /** Offer a hash; returns true if it verified; false otherwise.
     Once it cannot be verified (no sibling or parent), the hash
     is remembered, while returning false. */
    bool            OfferHash (bin64_t pos, const Sha1Hash& hash);
    /** Offer data; the behavior is the same as with a hash:
     accept or remember or drop. Returns true => ACK is sent. */
    bool            OfferData (bin64_t bin, const char* data, size_t length);
    /** Not implemented yet. */
    int             AppendData (char* data, int length) ;
    
    int             file_descriptor () const { return fd_; }
    int             peak_count () const { return peak_count_; }
    bin64_t         peak (int i) const { return peaks_[i]; }
    const Sha1Hash& peak_hash (int i) const { return peak_hashes_[i]; }
    bin64_t         peak_for (bin64_t pos) const;
    const Sha1Hash& hash (bin64_t pos) const {return hashes_[pos];}
    const Sha1Hash& root_hash () const { return root_hash_; }
    uint64_t        size () const { return size_; }
    uint64_t        packet_size () const { return sizek_; }
    uint64_t        complete () const { return complete_; }
    uint64_t        packets_complete () const { return completek_; }
    uint64_t        seq_complete () ;
    bool            is_complete () 
        { return size_ && complete_==size_; }
    bins&           ack_out () { return ack_out_; }
	
	~HashTree ();

	
};

}

#endif
