/*
 *  sbit.cpp
 *  binmap, a hybrid of bitmap and binary tree
 *
 *  Created by Victor Grishchenko on 3/28/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef BINS_H
#define BINS_H
#include "bin64.h"

class iterator; // FIXME shame

/** A binmap covering 2^64 range. Binmap is a hybrid of a bitmap (aka
    bit vector) and a binary tree. The key ability of a binmap is
    the aggregation of solid (all-0 or all-1) ranges. */
class binmap_t {
    
public:
    /** Need a 3-valued logic as a range might be either all-0 or all-1
        or some mix. In fact, any value different from 0x0 or 0xffff
        must be interpreted as MIXED. 
        All read/write operations on a binmap are made in terms of
        aligned binary intervals (bins).
     */
    typedef enum { FILLED=0xffff, EMPTY=0x0000, MIXED=0x5555 } fill_t;
    static const int NOJOIN;
    
    binmap_t();
    
    /** Copying constructor. */
    binmap_t(const binmap_t& b);
    
    /** Destructor. */
    ~binmap_t(); 

    /** Get value for the bin. */
    uint16_t    get (bin64_t bin); 
    
    /** Set value for the bin. */
    void        set (bin64_t bin, fill_t val=FILLED); 
    
    typedef enum {
        OR_OP,
        AND_OP,
        REMOVE_OP,
        COPY_OP
    } bin_op_t;
    
    /** Copy a range from another binmap. */
    void        range_op (binmap_t& mask, bin64_t range, bin_op_t op);
    void        range_copy (binmap_t& mask, bin64_t range)
        {   range_op(mask, range, COPY_OP);   }
    void        range_remove (binmap_t& mask, bin64_t range)
        {   range_op(mask, range, REMOVE_OP);   }
    void        range_or (binmap_t& mask, bin64_t range)
        {   range_op(mask, range, OR_OP);   }
    void        range_and (binmap_t& mask, bin64_t range)
        {   range_op(mask, range, AND_OP);   }
    
    /** Find the leftmost bin within the specified range which is
        either filled or empty. */
    bin64_t     find (const bin64_t range, fill_t seek=EMPTY) ;
    
    /** Find the leftmost bin within the specified range which is
        either filled or empty. Bins set to 1 in the filter binmap cannot
        be returned. In fact, this is an incremental bitwise op. */
    bin64_t     find_filtered
        (binmap_t& filter, bin64_t range, fill_t seek=EMPTY) ;
    
    /** Bitwise SUB; any bins set to one in the filter binmap should
        be set to 0 in this binmap. */
    void        remove (binmap_t& filter);
    
    void        dump(const char* note);

    /** Represent the binmap as a sequence of 0 and 1 stripes; for each
        new stripe only the starting offset is given. The first stripe
        is supposed to be empty (if the (0,0) bin is actually filled,
        the next stripe will also start at 0). */
    uint64_t*   get_stripes (int& count);

    /** Return the number of cells allocated in the binmap. */
    uint32_t    size() { return cells_allocated; }
    
    uint64_t    seq_length ();
    
    /** Return the topmost solid bin which covers the specified bin. */
    bin64_t     cover(bin64_t val);

    uint64_t    mass ();
    
    /** Return true if the range is solid (either all-0 or 1). If val is
        specified, the interval must be both solid and filled/empty,
        depending on the value. */
    bool        is_solid (bin64_t range=bin64_t::ALL, fill_t val=MIXED) ;
    /** Whether range/bin is empty. */
    bool        is_empty (bin64_t range=bin64_t::ALL) { return is_solid(range,EMPTY); }
    /** Whether range/bin is filled. */
    bool        is_filled (bin64_t range=bin64_t::ALL) { return is_solid(range,FILLED); }

    /** Clear everything, empty all bins. */
    void        clear ();
    
    /** Returns whether the int is mixed (not all-1 or all-0). */
    static bool is_mixed (uint16_t val) { return val!=EMPTY && val!=FILLED; }
    /** Returns whether the int is solid (0x0 or 0xffff). */
    static bool is_solid (uint16_t val) { return val==EMPTY || val==FILLED; }

    /** Twisting is some generalization of XOR. For every 1-bit in the mask,
        the respective layer of the binary tree is flipped, i.e. left and
        right change places. Twisting is mostly needed for randomization.  */
    void        twist (uint64_t mask);
    
    /** Convert binmap to a conventional flat bitmap; only bits corresponding
        to solid filled bins are set to 1.
        @param range  the bin (the range) to cover
        @param height aggregation level; use 2**height bins (2**height base
                layer bits per one bitmap bit). 
        @param bits   uint16_t array to put bitmap into; must have enough
                      of space, i.e. 2**(range.layer()-height-4) cells.  */
    void        to_coarse_bitmap (uint16_t* bits, bin64_t range, uint8_t height);
    
private:
    
    /** Every 16th uint32 is a flag field denoting whether
     previous 30 halves (in 15 cells) are deep or not.
     The last bit is used as a fill-flag.
     Free cells have a value of 0; neither deep nor flat
     cell could have a value of 0 except for the root
     cell in case the binmap is all-0. */
    union {
        uint32_t    *cells;
        uint16_t    *halves;
    };
    uint32_t    blocks_allocated;
    uint32_t    cells_allocated;
    int         height;
    uint64_t    twist_mask;
    uint16_t    free_top;
    
    void extend();
    
    static const uint8_t    SPLIT[16];
    static const uint8_t    JOIN[16];
    
    bool        deep(uint32_t half) const {
        return cells[(half>>1)|0xf] & (1<<(half&0x1f));
    }
    void        mark(uint32_t half) {
        cells[(half>>1)|0xf] |= (1<<(half&0x1f));
    }
    void        unmark(uint32_t half) {
        cells[(half>>1)|0xf] &= ~(1<<(half&0x1f));
    }
    
    void        extend_range();
    
    uint16_t    alloc_cell ();
    void        free_cell (uint16_t cell);
    
    /** Join the cell this half is pointing to
     (in other words, flatten the half). */
    bool        join(uint32_t half) ;
    
    /** Make the half deep. */
    void        split(uint32_t half) ;
    
    static uint32_t split16to32(uint16_t half);
    static int join32to16(uint32_t cell);

    void        map16 (uint16_t* target, bin64_t range);
    
    friend class iterator;
#ifdef FRIEND_TEST
    FRIEND_TEST(BinsTest,Routines);
#endif
};


/** Iterates over bins; for deep halves, bin==half.
 For flat halves, bin is a range of bits in a half.
 Iterator may split cells if needed.
 Value is either undefined (deep cell, mixed cell)
 or FILLED/EMPTY. */
class iterator {
public: // rm this
    binmap_t        *host;
    uint32_t    history[64];
    uint32_t    half;
    uint8_t     layer_;
    bin64_t     pos;  // TODO: half[] layer bin
public:
    iterator(binmap_t* host, bin64_t start=bin64_t(0,0), bool split=false);
    ~iterator();
    bool deep () { return host->deep(half); }
    bool solid () { 
        return !deep() && binmap_t::is_solid(host->halves[half]); 
    }
    void sibling () { half^=1; pos=pos.sibling(); }
    bool end () { return half==1; }
    void to (bool right);
    void left() {to(0);}
    void right() {to(1);}
    /** Move to the next defined (non-deep, flat) cell.
        If solid==true, move to a solid (0xffff/0x0) cell. */
    bin64_t next (bool stop_undeep=true, bool stop_solid=false, uint8_t stop_layer=0);
    bin64_t next_solid () { return next(false, true,0); }
    bin64_t bin() { return pos; }
    void towards(bin64_t bin) {
        bin64_t next = pos.towards(bin);
        assert(next!=bin64_t::NONE);
        to(next.is_right());
    }
    void parent() ;
    bool defined() { return !host->deep(half); }
    uint16_t& operator* () { return host->halves[half]; }
    uint8_t layer() const { return layer_; }
};


class binheap {
    bin64_t     *heap_;
    uint32_t    filled_;
    uint32_t    size_;
public:
    binheap();
    bin64_t pop();
    void    push(bin64_t);
    bool    empty() const { return !filled_; }
    void    extend();
    ~binheap();
};


#endif
