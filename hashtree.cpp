/*
 *  hashtree.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */

#include "hashtree.h"
#include <openssl/sha.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>

using namespace std;

#define HASHSZ 20
const size_t Sha1Hash::SIZE = HASHSZ;
const Sha1Hash Sha1Hash::ZERO = Sha1Hash();

Sha1Hash::Sha1Hash(const Sha1Hash& left, const Sha1Hash& right) {
	uint8_t data[HASHSZ*2];
	memcpy(data,left.bits,SIZE);
	memcpy(data+SIZE,right.bits,SIZE);
	SHA1(data,SIZE*2,bits);
}

Sha1Hash::Sha1Hash(const uint8_t* data, size_t length) {
	SHA1(data,length,bits);
}

Sha1Hash::Sha1Hash(const char* str) {
	SHA1((const unsigned char*)str,strlen(str),bits);
}

Sha1Hash::Sha1Hash(bool hex, const char* hash) {
	assert(!hex);
	memcpy(bits,hash,SIZE);
}

string	Sha1Hash::hex() {
	char hex[HASHSZ*2+1];
	for(int i=0; i<HASHSZ; i++)
		sprintf(hex+i*2, "%02x", bits[i]);
	return string(hex,HASHSZ*2);
}



/*void	HashTree::expand (bin tolen) {
	if (bits)
		munmap(bits,length*HASHSZ);
	length = tolen;
	status.resize(length);
	bits = (Sha1Hash*) mmap(NULL,length*HASHSZ,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
}*/


/*Sha1Hash HashTree::deriveRoot () {
	int i = peaks.size()-1;
	bin p = peaks[i].first;
	Sha1Hash hash = peaks[i].second;
	i--;
	while (p<bin::ALL) {
		if (p.is_left()) {
			p = p.parent();
			hash = Sha1Hash(hash,Sha1Hash::ZERO);
		} else {
			if (i<0 || peaks[i].first!=p.sibling())
				return Sha1Hash::ZERO;
			hash = Sha1Hash(peaks[i].second,hash);
			p = p.parent();
			i--;
		}
	}
	return hash;
}

HashTree::HashTree (int fd)  {
	//fd = open(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd<0) return;
	struct stat st;
	fstat(fd, &st);
	length = (st.st_size>>10) + (st.st_size%1024 ? 1 : 0);
	mass = bin::lenpeak(length); // incorrect
	bits.resize(mass+1);
	status.resize(mass+1);
	uint8_t buf[1024];
	for(bin i=1; i<=mass; i++)
		if (i.layer()) {
			bits[i] = Sha1Hash(bits[i.left()],bits[i.right()]);
		} else {
			int len = pread(fd,buf,1024,i.offset()<<10);
			bits[i] = Sha1Hash(buf,len);
		}
	//close(fd);
	bin::vec p = bin::peaks(length);
	while(p.size()) {
		peaks.push_back(binhash(p.front(),bits[p.front()]));
		p.pop_front();
	}
	root = deriveRoot();
}

HashTree::HashTree (const Sha1Hash& with_root) : root(with_root), length(0), mass(0) {
	// recover the partially filled hash file
	// first, size
	// then, peaks
	// works? then offer the rest
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

*/
