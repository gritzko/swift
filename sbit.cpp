/*
 *  sbit.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 4/1/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "sbit.h"

uint16_t		bins::SPLIT[256];
uint8_t			bins::JOIN[256];
uint16_t		bins::OFFMASK[32];
static uint16_t NO_PARENT = 0xffff;

bins::bins () : peak(31), bits(32,0), deep(32,false), prnt(16,NO_PARENT), 
	allocp(1), rescan_flag(true) {
	prnt[0] = 0;
	deep[1] = true;
}

bins::bins(const bins& b) : peak(b.peak), allocp(b.allocp),
		bits(b.bits), prnt(b.prnt), deep(b.deep), rescan_flag(b.rescan_flag) 
{
}



void	bins::init () {
	for(int i=0; i<256; i++)
		JOIN[i] = 0xff;
	for(int i=0; i<256; i++) {
		int split = 0;
		for(int b=0; b<8; b++)
			if (i&(1<<b))
				split |= (1<<(2*b)) | (1<<(2*b+1));
		SPLIT[i] = split;
		JOIN[split&0xff] = i&0xf;
	}
	for(bin i=0; i<32; i++) {
		int m = 0;
		for(int j=i.offset(); j<i.length(); j++)
			m |= 1<<j;
		OFFMASK[i] = m;
	}
}


void	bins::unlink (int half) {
	int s[32], sp=0;
	s[sp++] = half;
	while (sp) {
		int h = s[--sp];
		if (deep[h]) {
			int c=bits[h], l=c<<1, r=l+1;
			prnt[c] = NO_PARENT;
			deep[h]=false;
			s[sp++] = l;
			s[sp++] = r;
		}
	}
}


bool	bins::get(bin pos) const {
	if (pos>peak)
		return false;
	chunk_iterator i(const_cast<bins*>(this));
	while (i.deep() && i.chunk_top()>pos) 
		i.to(pos);
	if (i.deep())
		return false;
	int l = OFFMASK[pos.scoped(i.chunk_top(),4)];
	return (*i & l) == l;
}


bool	bins::clean(bin pos) const {
	if (pos>peak)
		return bin::all1(pos) ? clean(peak) : true;
	chunk_iterator i(const_cast<bins*>(this));
	while (i.deep() && i.chunk_top()>pos) 
		i.to(pos);
	if (i.deep())
		return false;
	int l = OFFMASK[pos.scoped(i.chunk_top(),4)];
	return (*i & l) == 0;
}


void	bins::expand () {
	int oldrootcell = cell_alloc();
	if (deep[0])
		prnt[bits[0]] = oldrootcell;
	prnt[oldrootcell] = 0;
	int orl = oldrootcell<<1, orr = orl+1;
	bits[orl] = bits[0];
	bits[orr] = 0;
	bits[0] = oldrootcell;
	deep[orl] = deep[0];
	deep[orr] = false;
	deep[0] = true;
	peak = peak.parent();
	compact(0);
}


void	bins::set (bin pos, bool to) {
	if (!pos)
		return;
	while (pos>peak)
		expand(); 
	chunk_iterator i(this);
	while (i.chunk_top().layer()>pos.layer()+4)
		i.to(pos);
	while (i.deep() && i.chunk_top().layer()>pos.layer())
		i.to(pos);
	if (i.deep())
		unlink(i.half);
	int mask = OFFMASK[pos.scoped(i.chunk_top(),4)];
	if (to)
		*i |= mask;
	else
		*i &= ~mask;
	while(i.up()); //compact
}


bool	bins::compact (int half) {
	if (!deep[half])
		return false;
	int l = bits[half]<<1, r = l+1;
	if (deep[l] || deep[r])
		return false;
	int l1 = JOIN[bits[l]&0xff], l2 = JOIN[bits[l]>>8];
	if (l1==0xff || l2==0xff)
		return false;
	int r1 = JOIN[bits[r]&0xff], r2 = JOIN[bits[r]>>8];
	if (r1==0xff || r2==0xff)
		return false;
	deep[half] = false;
	prnt[bits[half]] = NO_PARENT;
	deep[l] = deep[r] = false;//coward
	bits[half] = (l1) | (l2<<4) | (r1<<8) | (r2<<12);
	return true;
}


void	bins::split (int half) {
	if (!deep[half]) {
		int newcell = cell_alloc(), oldcell=half>>1;
		int l = newcell<<1, r = l+1;
		bits[l] = SPLIT[bits[half]&0xff];
		bits[r] = SPLIT[bits[half]>>8];
		deep[half] = true;
		bits[half] = newcell;
		prnt[newcell] = oldcell;
	}
}


void	bins::doop (bins& b, int op) {
	while (b.peak<peak)
		b.expand();
	while (b.peak>peak)
		expand();
	chunk_iterator i(this), j(&b);
	do {
		while (i.deep() || j.deep()) {
			i.left();
			j.left();
		}
		switch (op) {
			case OR_OP: (*i) |= *j; break;
			case AND_OP: (*i) &= *j; break;
			case SUB_OP: (*i) &= ~*j; break;
		}
		while (i.chunk_top().is_right()) {
			i.up();
			j.up();
		}
		i.up();
		j.up();
		i.right();
		j.right();
	} while (!i.end());
}


void	bins::operator |= (bins& b) {
	doop(b,OR_OP);
}


void	bins::operator &= (bins& b) {
	doop(b,AND_OP);
}


void	bins::operator -= (bins& b) {
	doop(b,SUB_OP);
}



int	bins::cell_alloc () { // FIXME: 0xffff size too big
	while (allocp<prnt.size() && prnt[allocp]!=NO_PARENT)
		allocp++;
	if (allocp==prnt.size()) {
		if (rescan_flag) {
			rescan_flag = false;
			allocp=0;
			return cell_alloc();
		} else {
			rescan_flag = true;
			bits.resize(allocp*4,0);
			prnt.resize(allocp*2,NO_PARENT);
			deep.resize(allocp*4,false);
		}
	}
	deep[allocp*2] = false;
	deep[allocp*2+1] = false;
	prnt[allocp] = NO_PARENT;
	return allocp;
}



/*void	bins::make_space () {		WAY TOO SMART, DO LATER
 std::vector<int> renames(allocp), irenames(allocp);
 int newcellsize=0;
 for(int i=0; i<allocp; i++) 
 if (prnt[i]) {
 renames[newcellsize] = i;
 irenames[i] = newcellsize;
 newcellsize++;
 }
 if (newcellsize<bits.size()*3/4) {
 for (int i=0; i<newcellsize; i++) {
 int n=renames[i], l=i<<1, r=l+1, ln=n>>1, rn=ln+1;
 deep[l] = deep[ln];
 deep[r] = deep[rn];
 prnt[i] = irenames[prnt[n]];
 bits[l] = deep[l] ? irenames[bits[ln]] : bits[ln];
 bits[r] = deep[r] ? irenames[bits[rn]] : bits[rn];
 }
 allocp = newcellsize;
 }
 if (allocp>bits.size()/2) {
 deep.resize(bits.size()*2);
 prnt.resize(bits.size());
 bits.resize(bits.size()*2);
 }
 }*/