/*
 *  sbit.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/28/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef BINS_H
#define BINS_H
#include "bin64.h"
#include <gtest/gtest.h>

/**  A binmap covering 2^64 range. Complexity limit: 100+200LoC  */
class bins {
    
public:
    typedef enum { FILLED=0xffff, EMPTY=0x0000 } fill_t;
    static const int NOJOIN;
    
    bins();
    
    bins(const bins& b);
    
    uint16_t    get (bin64_t bin); 
    
    void        set (bin64_t bin, fill_t val=FILLED); 
    
    void        copy_range (bins& origin, bin64_t range);
    
    bin64_t     find (const bin64_t range, const uint8_t layer, fill_t seek=EMPTY) ;
    
    bin64_t     find_filtered
        (bins& filter, bin64_t range, const uint8_t layer, fill_t seek=EMPTY) ;
    
    void        remove (bins& b);
    
    void        dump(const char* note);

    uint64_t*   get_stripes (int& count);

    uint32_t    size() { return cells_allocated; }
    
    uint64_t    seq_length ();
    
    bin64_t     cover(bin64_t val);

    uint64_t    mass ();
    
    bool        is_empty () const { return !deep(0) && !halves[0]; }

    void        clear ();
    
    static bool is_mixed (uint16_t val) { return val!=EMPTY && val!=FILLED; }

    void        twist (uint64_t mask);
    
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
    uint32_t    ap;
    uint64_t    twist_mask;
    
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
    
    void extend_range();
    
    uint16_t    alloc_cell ();
    void        free_cell (uint16_t cell);
    
    /** Join the cell this half is pointing to
     (in other words, flatten the half). */
    bool        join(uint32_t half) ;
    
    /** Make the half deep. */
    void        split(uint32_t half) ;
    
    static uint32_t split16to32(uint16_t half);
    static int join32to16(uint32_t cell);
    
    friend class iterator;
    FRIEND_TEST(BinsTest,Routines);
};


/** Iterates over bins; for deep halves, bin==half.
 For flat halves, bin is a range of bits in a half.
 Iterator may split cells if needed.
 Value is either undefined (deep cell, mixed cell)
 or FILLED/EMPTY. */
class iterator {
public: // rm this
    bins        *host;
    uint32_t    history[64];
    uint32_t    half;
    uint8_t     layer_;
    bin64_t     pos;  // TODO: half[] layer bin
public:
    iterator(bins* host, bin64_t start=0, bool split=false);
    ~iterator();
    bool deep () { return host->deep(half); }
    bool solid () { 
        return !deep() && (host->halves[half]==bins::FILLED || 
                host->halves[half]==bins::EMPTY); 
    }
    void sibling () { half^=1; pos=pos.sibling(); }
    bool end () { return half==1; }
    void to (bool right);
    void left() {to(0);}
    void right() {to(1);}
    /** Move to the next defined (non-deep, flat) cell.
        If solid==true, move to a solid (0xffff/0x0) cell. */
    bin64_t next (bool solid=false);
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
