/*
 *  bin64.h
 *  bin numbers (binaty tree enumeration/navigation)
 *
 *  Created by Victor Grishchenko on ??/09/09 in Karlovy Vary
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef BIN64_H
#define BIN64_H
#include <assert.h>
#include "compat.h"


/** Numbering for (aligned) logarithmical bins.
    Each number stands for an interval
    [o*2^l,(o+1)*2^l), where l is the layer and o
    is the offset.
    Bin numbers in the tail111 encoding: meaningless
    bits in the tail are set to 0111...11, while the
    head denotes the offset. Thus, 1101 is the bin
    at layer 1, offset 3 (i.e. fourth). 
    Obviously, bins form a binary tree. All navigation
    is made in terms of binary trees: left, right,
    sibling, parent, etc.
 */
struct bin64_t {
    uint64_t v;
    static const uint64_t NONE;
    static const uint64_t ALL;
    static const uint32_t NONE32;
    static const uint32_t ALL32;

    bin64_t() : v(NONE) {}
    bin64_t(const bin64_t&b) : v(b.v) {}
    bin64_t(const uint32_t val) ;
    bin64_t(const uint64_t val) : v(val) {}
    bin64_t(uint8_t layer, uint64_t offset) :
        v( (offset<<(layer+1)) | ((1ULL<<layer)-1) ) {}
    operator uint64_t () const { return v; }
    uint32_t to32() const ;
    bool operator == (bin64_t& b) const { return v==b.v; }

    static bin64_t none () { return NONE; }
    static bin64_t all () { return ALL; }

    uint64_t tail_bits () const {
        return v ^ (v+1);
    }

    uint64_t tail_bit () const {
        return (tail_bits()+1)>>1;
    }

    /** Get the sibling interval in the binary tree. */
    bin64_t sibling () const {
        // if (v==ALL) return NONE;
        return bin64_t(v^(tail_bit()<<1));
    }

    int layer () const {
        int r = 0;
        uint64_t tail = ((v^(v+1))+1)>>1;
        if (tail>0xffffffffULL) {
            r = 32;
            tail>>=32;
        }
        // courtesy of Sean Eron Anderson
        // http://graphics.stanford.edu/~seander/bithacks.html
        static const int DeBRUIJN[32] = {
          0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
          31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
        };
        r += DeBRUIJN[((uint32_t)(tail*0x077CB531U))>>27];
        return r;
    }

    /** Get the bin's offset in base units, i.e. 4 for (1,2). */
    uint64_t base_offset () const {
        return (v&~(tail_bits()))>>1;
    }

    /** Get the bin's offset at its own layer, e.g. 2 for (1,2). */
    uint64_t offset () const {
        return v >> (layer()+1);
    }

    /** Get a child bin; either right(true) or left(false). */
    bin64_t to (bool right) const {
        if (!(v&1))
            return NONE;
        uint64_t tb = ((tail_bits() >> 1) + 1) >> 1;
        if (right)
            return bin64_t(v + tb);
        return bin64_t(v ^ tb);
    }

    /** Get the left child bin. */
    bin64_t left () const {
        return to(false);
    }

    /** Get the right child bin. */
    bin64_t right () const {
        return to(true);
    }

    /** Check whether this bin is within the specified bin. */
    bool    within (bin64_t maybe_asc) {
        if (maybe_asc==bin64_t::NONE)
            return false;
        uint64_t short_tail = maybe_asc.tail_bits();
        if (tail_bits()>short_tail)
            return false;
        return (v&~short_tail) == (maybe_asc.v&~short_tail) ;
    }

    /** Left or right, depending whether the destination is. */
    bin64_t towards (bin64_t dest) const {
        if (!dest.within(*this))
            return NONE;
        if (dest.within(left()))
            return left();
        else
            return right();
    }

    /** Twist/untwist a bin number according to the mask. */
    bin64_t twisted (uint64_t mask) const {
        return bin64_t( v ^ ((mask<<1)&~tail_bits()) );
    }

    /** Get the paretn bin. */
    bin64_t parent () const {
        uint64_t tbs = tail_bits(), ntbs = (tbs+1)|tbs;
        return bin64_t( (v&~ntbs) | tbs );
    }

    /** Check whether this bin is the left sibling. */
    inline bool is_left () const {
        uint64_t tb = tail_bit();
        return !(v&(tb<<1));
    }
    
    /** Check whether this bin is the right sibling. */
    inline bool is_right() const { return !is_left(); }

    /** Get the leftmost basic bin within this bin. */
    bin64_t left_foot () const {
        if (v==NONE)
            return NONE;
        return bin64_t(0,base_offset());
    }

    /** Whether layer is 0. */
    bool    is_base () const {
        return !(v & 1);
    }

    /** Depth-first in-order binary tree traversal. */
    bin64_t next_dfsio (uint8_t floor);

    /** Return the number of basic bins within this bin. */
    bin64_t width () const {
        return (tail_bits()+1)>>1;
    }
    
    /** Get the standard-form null-terminated string
        representation of this bin, e.g. "(2,1)".
        The string is statically allocated, must
        not be reused or released. */
    const char* str () const;

    /** The array must have 64 cells, as it is the max
     number of peaks possible +1 (and there are no reason
     to assume there will be less in any given case. */
    static int peaks (uint64_t length, bin64_t* peaks) ;

};


#endif

/**
                 00111
       0011                    1011
  001      101         1001          1101
0   10  100  110    1000  1010   1100   1110

                  7
      3                         11
  1        5             9             13
0   2    4    6       8    10      12     14

once we have peak hashes, this struture is more natural than bin-v1

*/
