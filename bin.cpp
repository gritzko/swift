/*
 *  bin.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include "bin.h"
#include <algorithm>

bin bin::NONE = 0;
bin bin::ALL = 0x7fffffff;
uint8_t bin::BC[256] = {};
uint8_t bin::T0[256] = {};

void bin::init () {
	for(int i=0; i<256; i++) {
		int bc=0, bit;
		for(bit=0; bit<8; bit++)
			if ((i>>bit)&1) bc++;
		BC[i] = bc;
		for(bit=0; bit<8 && ((i>>bit)&1)==0; bit++);
		T0[i] = bit;
	}
}

bin::vec bin::peaks (uint32_t len) {
	bin::vec pks;
	uint32_t i=len, run=0;
	while (i) {
		uint32_t bit = bin::highbit(i);
		i^=bit;
		run |= bit;
		pks.push_back(lenpeak(run));
	}
	return pks;
}

void bin::order (vec* vv) {
	vec& v = *vv;
	std::sort(v.begin(),v.end());
	std::reverse(v.begin(),v.end());
	vec::iterator pw=v.begin(), pr=v.begin();
	while (pr!=v.end()) {
		*pw = *pr;
		while (pw!=v.begin() && (pw-1)->sibling()==*pw) {
			pw--;
			*pw = pw->parent();
		}
		bin skipto = *pw - pw->mass();
		while (pr!=v.end() && *pr>skipto) {
			pr++;
		}
		pw++;
	}
	v.resize(pw-v.begin());
}
