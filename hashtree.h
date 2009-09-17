/*
 *  hashtree.h
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#ifndef P2TP_SHA1_HASH_TREE_H
#define P2TP_SHA1_HASH_TREE_H
#include "bin.h"
#include <string.h>
#include <string>
#include <vector>

struct Sha1Hash {
	uint8_t	bits[20];

	Sha1Hash() { memset(bits,0,20); }
	Sha1Hash(const Sha1Hash& left, const Sha1Hash& right);
	Sha1Hash(const uint8_t* bits, size_t length);
	/***/
	Sha1Hash(const char* bits);
	Sha1Hash(bool hex, const char* hash);
	
	std::string	hex();
	bool	operator == (const Sha1Hash& b) const
		{ return 0==memcmp(bits,b.bits,SIZE); }
	bool	operator != (const Sha1Hash& b) const { return !(*this==b); }
	
	const static Sha1Hash ZERO;
	const static size_t SIZE;
};

typedef std::pair<bin,Sha1Hash> binhash;

struct HashTree {
	Sha1Hash	root;
	int			fd;
	bin			mass;
	uint32_t	length;
	std::vector<bool> status;
	std::vector<Sha1Hash> bits;
	std::vector<binhash> peaks;
	typedef enum { ACCEPT, DUNNO, PEAK_ACCEPT, REJECT } hashres_t;
protected:
	Sha1Hash		deriveRoot();
	hashres_t		offerPeak (bin pos, Sha1Hash hash);
public:
	
	HashTree (int fd);
	HashTree (const Sha1Hash& root);
	
	~HashTree ();

	hashres_t		offer (bin pos, const Sha1Hash& hash);
	
	bool			rooted () const { return length>0; }
	
	const Sha1Hash&	operator [] (bin i) { 
		return i<=mass ? bits[i] : Sha1Hash::ZERO; 
	}
	
	uint32_t		data_size () const { return length; }
	
	bin				data_mass () const { return mass; }
	
	const std::vector<binhash>& peak_hashes() const { return peaks; }
	
};

#endif
