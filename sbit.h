/*
 *  sbit.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/28/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#ifndef SERP_SBIT_H
#define SERP_SBIT_H
#include <vector>
#include "bin.h"

class bins {

public:

	class bin_iterator;
	
	/**	Traverses 16-bit chunks. */
	class	chunk_iterator {
		bins*	host;
		bin		top;
		int		half;
		
		bool	up() { 
			top=top.parent();
			int cell = half>>1;
			half=host->prnt[cell]<<1;
			if (!host->deep[half] || host->bits[half]!=cell)
				half++;
			assert(host->deep[half] && host->bits[half]==cell);
			return host->compact(half);
		}
		void	left() {
			assert(top.layer()>4);
			top = top.left();
			if (!deep())
				host->split(half);
			half = host->bits[half]<<1;
		}
		void	right() {
			assert(top.layer()>4);
			top = top.right();
			if (!deep())
				host->split(half);
			half = (host->bits[half]<<1)+1;
		}
		void	to(bin target) {
			assert(top.layer()>4);
			bin next = top.child(target);
			if (next.is_left())
				left();
			else
				right();
		}
		int		cell () const { return half>>1; }
		bool	deep() const { return host->deep[half]; }
		bool	is_right () const { return half&1; }
		bool	end () const { return half==1; }

	public:
		chunk_iterator(bins* h, int hlf=0) : host(h), top(h->peak), half(hlf) {
			//while (deep())
			//	left();
		}
		void operator ++ () {
			while (is_right())
				up();
			up();
			right();
			while (deep() && !end())
				left();
		}
		uint16_t& operator * () {
			return host->bits[half];
		}
		bool operator == (const bins::chunk_iterator& b) const { 
			return host==b.host && half==b.half;
		}
		bin	chunk_top() const { return top; }
		friend class bins::bin_iterator;
		friend class bins;
	}; // chunk_iterator
	
	
	/**	Traverses bins.	*/
	class	bin_iterator {
		bins::chunk_iterator i;
		bin cur;
	public:
		bin_iterator(chunk_iterator ci, bin pos=0) : i(ci), cur(pos) {
			while (!i.end() && i.deep())
				i.left();
			++(*this);
		}
		bin operator * () const {
			return cur.unscoped(i.top,4);
		}
		void operator ++ () {
			if (i.end())
				return;
			do {
				if (cur<bin(4,0)) {
					cur++;
				} else {
					cur = 1;
					++i;
				}
			} while (!i.end() && (*i&OFFMASK[cur])!=OFFMASK[cur]);
			bin p=cur.parent(); 
			while (p<=bin(4,0) && (*i&OFFMASK[p])==OFFMASK[p]) {
				cur=p;
				p=cur.parent();
			}
		}
		bool operator == (const bins::chunk_iterator& b) const {return i==b;}
		bool operator == (const bins::bin_iterator& b) const {
			return i==b.i && cur==b.cur;
		}
		bool operator != (const bins::bin_iterator& b) const { return !(*this==b); }
	}; //	bin_iterator
	
	
private:
	bin						peak;
	std::vector<uint16_t>	bits;
	std::vector<uint16_t>	prnt; // BAD BAD BAD
	std::vector<bool>		deep;
	int						allocp;
	bool					rescan_flag;

	
private:
	
	void	unlink (int half);
	void	expand();
	void	split(int half);
	
	//void	make_space();
	int		cell_alloc();
	bool	compact (int cell);
	
	void	doop (bins& b, int op);
	
	static uint16_t	SPLIT[256];
	static uint8_t	JOIN[256];
	static uint16_t	OFFMASK[32];
	typedef enum { AND_OP, OR_OP, SUB_OP } ops_t;
	
public:
	
	bins();
	bins(const bins& orig);
	
	bool	get(bin pos) const;
	bool	clean(bin pos) const;
	bool	contains(bin pos) const { return get(pos); }
	void	set(bin pos, bool to=true);
	bool	empty() const { return !deep[0] && !bits[0]; }
	bool	operator [] (bin pos) const {return get(pos);}
	void	operator |= (bin pos) { set(pos); }
	void	operator -= (bin pos) { set(pos,false); }
	void	operator |= (bins& b);
	void	operator &= (bins& b);
	void	operator -= (bins& b);
	
	bin_iterator begin() { return bin_iterator(chunk_iterator(this,0)); }
	bin_iterator end() { return bin_iterator(chunk_iterator(this,1),1); }
	
	static	void init();
	friend class SbitTest;

};


#endif