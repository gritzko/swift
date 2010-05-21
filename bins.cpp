/*
 *  sbit.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 4/1/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <utility>

#include "bins.h"

#undef max

// make it work piece by piece

const uint8_t    binmap_t::SPLIT[16] = 
{0, 3, 12, 15, 48, 51, 60, 63, 192, 195, 204, 207, 240, 243, 252, 255};
const uint8_t    binmap_t::JOIN[16] =
{0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15};
const int binmap_t::NOJOIN = 0x10000;


void binmap_t::extend () {
    const size_t nblocks = (blocks_allocated != 0) ? (2 * blocks_allocated) : (1);

    if( 16 * nblocks > 1 + std::numeric_limits<uint16_t>::max() )
        return /* The limit of cells number reached */;

    uint32_t * const ncells = (uint32_t *) realloc(cells, nblocks * 16 * sizeof(uint32_t));
    if( ncells == NULL )
        return /* Memory allocation error */;

    size_t blk = nblocks;
    while( blk-- != blocks_allocated ) {
        uint16_t const blk_off =  16 * blk;
        uint16_t * const blk_ptr = reinterpret_cast<uint16_t *>(ncells + blk_off);

        blk_ptr[28] = free_top;

        for(uint16_t i = 13; i != (uint16_t)-1; --i)
            blk_ptr[2 * i] = blk_off + i + 1;

        free_top = blk_off;
    }

    blocks_allocated = nblocks;
    cells = ncells;
}

binmap_t::binmap_t() :  height(4), blocks_allocated(0), cells(NULL), 
                free_top(0), cells_allocated(0), twist_mask(0) {
    alloc_cell();
    assert( free_top == 1 );
}

binmap_t::~binmap_t () {
    if (cells)
        free(cells);
}

void binmap_t::twist (uint64_t mask) {
    while ( (1<<height) <= mask )
        extend_range();
    twist_mask = mask;
}

binmap_t::binmap_t (const binmap_t& b) : height(b.height), free_top(b.free_top),
blocks_allocated(b.blocks_allocated), cells_allocated(b.cells_allocated) {
    size_t memsz = blocks_allocated*16*32;
    cells = (uint32_t*) malloc(memsz);
    memcpy(cells,b.cells,memsz);
}

void binmap_t::dump (const char* note) {
    printf("%s\t",note);
    for(int i=0; i<(blocks_allocated<<5); i++) {
        if ( (i&0x1f)>29 )
            printf("|%x ",halves[i]);
        else if (deep(i))
            printf(">%i ",halves[i]);
        else
            printf("%x ",halves[i]);
        if (i&1)
            printf(" ");
    }
    printf("\n");
}

uint32_t binmap_t::split16to32(uint16_t halfval) {
    uint32_t nval = 0;
    for(int i=0; i<4; i++) {
        nval >>= 8;
        nval |= (SPLIT[halfval&0xf])<<24;
        halfval >>= 4;
    }
    return nval;
}


int binmap_t::join32to16(uint32_t cval) {
    union { uint32_t i; uint8_t a[4]; } uvar;
    uvar.i = cval & (cval>>1) & 0x55555555;
    if ( (uvar.i|(uvar.i<<1)) != cval )
        return NOJOIN;
    uvar.i = (uvar.i&0x05050505) | ((uvar.i&0x50505050U)>>3);
    uint16_t res = 0;
    for(int i=3; i>=0; i--) {
        res <<= 4;
        res |= JOIN[uvar.a[i]];
    }
    return res;
}


void        binmap_t::split (uint32_t half) {
    if (deep(half))
        return;
    uint32_t cell = alloc_cell(), left=cell<<1, right=left+1;
    mark(half); //cells[(half>>1)|0xf] |= 1<<(half&0x1f);
    uint16_t halfval = halves[half];
    uint32_t nval = split16to32(halfval);
    halves[left] = nval&0xffff;
    halves[right] = nval>>16;
    halves[half] = cell;
}


bool        binmap_t::join (uint32_t half) {
    uint32_t cellno = halves[half];
    int left = cellno<<1, right=left+1;
    if (deep(left) || deep(right))
        return false;
    int res = join32to16(cells[cellno]);
    if (res>0xffff)
        return false;
    halves[half] = (uint16_t)res;
    unmark(half);
    free_cell(cellno);
    //cells[(half>>1)|0xf] &= ~(1<<(half&0x1f));
    //(*childdeepcell) &= 0xffff>>1; // clean the full bit
    return true;
}

void    binmap_t::free_cell (uint16_t cell) {
    --cells_allocated;

    halves[2 * cell] = free_top;
    free_top = cell;
}

/** Get a free cell. */
uint16_t    binmap_t::alloc_cell () {
    if( cells_allocated == 15 * blocks_allocated )
        extend();
    
    if( cells_allocated == 15 * blocks_allocated ) {
        assert( free_top != 0 );
        return 0;
    }
    
    ++cells_allocated;
    
    const uint16_t ref = free_top;
    free_top = halves[2 * ref];
    
    cells[ref] = 0;
    unmark(2 * ref);
    unmark(2 * ref + 1);
    
    return ref;
}


bin64_t iterator::next (bool stop_undeep, bool stop_solid, uint8_t stop_layer) {
    while (pos.is_right())
        parent();
    sibling();
    while (     (!stop_undeep || deep()) && 
                (!stop_solid || (deep() || !solid()) ) && 
                (layer()>stop_layer)      )
        left();
    return pos;
}


iterator::iterator(binmap_t* host_, bin64_t start, bool split) { 
    host = host_;
    half = 0;
    for(int i=0; i<64; i++)
        history[i] = 1;
    pos = bin64_t(host->height,0);
    layer_ = host->height;
    while (!start.within(pos))
        parent();
    while (pos!=start && (deep() || split))
        towards(start);
}


iterator::~iterator () {
    while (half>1 && !deep())
        parent();
    // PROBLEM: may hang in the air if two iters
    // exist simultaneously
    // FIX: iterators are not exposed (protected)
}


void iterator::to (bool right) {
    if (!deep())
        host->split(half);
    history[layer()] = half; // FIXME
    pos = pos.to(right);
    layer_--;
    if ( (host->twist_mask >> layer()) & 1 )
        right = !right; // twist it!
    half = (host->halves[half]<<1) + right;
}


void binmap_t::extend_range () {
    assert(height<62);
    height++;
    uint16_t newroot = alloc_cell();
    int left = newroot<<1, right = left+1;
    cells[newroot] = cells[0];
    halves[0] = newroot;
    halves[1] = 0;
    if (deep(0))
        mark(left);
    else
        mark(0);            
    if (deep(1)) {
        mark(right);
        unmark(1);
    }        
}

void iterator::parent () {
    if (!half) {
        host->extend_range();
        history[layer()+1] = 0;
    }
    pos = pos.parent();
    layer_++;
    half = history[layer()];
    host->join(half);
    //host->dump("| ");
}


bin64_t binmap_t::find (const bin64_t range, fill_t seek) {
    iterator i(this,range,true);
    fill_t stop = seek==EMPTY ? FILLED : EMPTY;
    while (true) {
        while ( i.deep() || (*i!=stop && *i!=seek) )
            i.left();
        if (!i.deep() && *i==seek)
            return i.bin();
        while (i.bin().is_right() && i.bin()!=range)
            i.parent();
        if (i.bin()==range)
            break;
        i.parent();
        i.right();
    }
    return bin64_t::NONE;
}


uint16_t binmap_t::get (bin64_t bin) {
    if (bin==bin64_t::NONE)
        return EMPTY;
    iterator i(this,bin,true);
    //while ( i.pos!=bin && 
    //        (i.deep() || (*i!=BIN_FULL && *i!=BIN_EMPTY)) )
    //    i.towards(bin);
    //printf("at %i ",i.half);
    //dump("get made");
    return *i; // deep cell is never 0xffff or 0x0000; FIXME: API caveat
}


void binmap_t::clear () {
    set(bin64_t(height,0),EMPTY);
}


uint64_t binmap_t::mass () {
    iterator i(this,bin64_t(0,0),false);
    uint64_t ret = 0;
    while (!i.solid())
        i.left();
    while (!i.end()) {
        if (*i==binmap_t::FILLED)
            ret += i.pos.width();
        i.next_solid();
    }
    return ret;
}


void binmap_t::set (bin64_t bin, fill_t val) {
    if (bin==bin64_t::NONE)
        return;
    assert(val==FILLED || val==EMPTY);
    iterator i(this,bin,false);
    while (i.bin()!=bin && (i.deep() || *i!=val))
        i.towards(bin);
    if (!i.deep() && *i==val)
        return;
    while (i.deep()) 
        i.left();
    do {
        *i = val;
        i.next();
    } while (i.bin().within(bin));
    // dump("just set");
}


uint64_t*   binmap_t::get_stripes (int& count) {
    int size = 32;
    uint64_t *stripes = (uint64_t*) malloc(32*8);
    count = 0;
    uint16_t cur = binmap_t::EMPTY;
    stripes[count++] = 0;
    iterator i(this,bin64_t(0,0),false);
    while (!i.solid())
        i.left();

    while (!i.end()) {

        if (cur!=*i) { // new stripe
            cur = *i;
            stripes[count++] = i.bin().base_offset();
            if (count==size) {
                size <<= 1;
                stripes = (uint64_t*) realloc(stripes,size*8);
            }
        }

        i.next_solid();

    }

    if ( !(count&1) )
        stripes[count++] = i.bin().base_offset();
    
    return stripes;
}


void    binmap_t::remove (binmap_t& b) {
    uint8_t start_lr = b.height>height ? b.height : height;
    bin64_t top(start_lr,0);
    iterator zis(this,top), zat(&b,top);
    while (!zis.end()) {
        while (zis.deep() || zat.deep()) {
            zis.left(); zat.left();
        }
        
        *zis &= ~*zat;
        
        while (zis.pos.is_right()) {
            zis.parent(); zat.parent();
        }
        zis.sibling(); zat.sibling();
    }
}


bin64_t     binmap_t::cover(bin64_t val) {
    if (val==bin64_t::NONE)
        return val;
    iterator i(this,val,false);
    while (i.pos!=val && !i.solid())
        i.towards(val);
    if (!i.solid())
        return bin64_t::NONE;
    return i.pos;
}


bin64_t     binmap_t::find_filtered 
    (binmap_t& filter, bin64_t range, fill_t seek)  
{
    if (range==bin64_t::ALL)
        range = bin64_t ( height>filter.height ? height : filter.height, 0 );
    iterator ti(this,range,true), fi(&filter,range,true);
    fill_t stop = seek==EMPTY ? FILLED : EMPTY;
    while (true) {
        while ( 
                fi.deep() ?
                (ti.deep() || *ti!=stop)  :
                (ti.deep() ? *fi!=FILLED : 
                    ( ((*ti^stop)&~*fi) && (*ti!=seek || *fi!=EMPTY) ) ) 
              ) 
        {
            ti.left(); fi.left();                
        }
        if (!ti.deep() && *ti==seek && !fi.deep() && *fi==EMPTY)
            return ti.bin();
        while (ti.bin().is_right() && ti.bin()!=range)
            ti.parent(), fi.parent();
        if (ti.bin()==range)
            break;
        ti.sibling(), fi.sibling();
    }
    return bin64_t::NONE;    
}

void        binmap_t::range_op (binmap_t& mask, bin64_t range, bin_op_t op) {
    if (range==bin64_t::ALL)
        range = bin64_t ( height>mask.height ? height : mask.height, 0 );
    iterator zis(this,range,true), zat(&mask,range,true);
    while (zis.pos.within(range)) {
        while (zis.deep() || zat.deep()) {
            zis.left(); zat.left();
        }

        switch (op) {
            case REMOVE_OP:
                *zis &= ~*zat;
                break;
            case AND_OP:
                *zis &= *zat;
                break;
            case COPY_OP:
                *zis = *zat;
                break;
            case OR_OP:
                *zis |= *zat;
        }
        
        while (zis.pos.is_right()) {
            zis.parent(); zat.parent();
        }
        zis.sibling(); zat.sibling();
    }
}

uint64_t    binmap_t::seq_length () {
    iterator i(this,bin64_t(height,0));
    if (!i.deep() && *i==FILLED)
        return i.pos.width();
    while (!i.pos.is_base()) {
        if (i.deep() || *i!=FILLED) 
            i.left();
        else
            i.sibling();
    }
    return i.pos.base_offset() + (*i==FILLED ? 1 : 0);
}


bool        binmap_t::is_solid (bin64_t range, fill_t val)  {
    if (range==bin64_t::ALL) 
        return !deep(0) && (is_mixed(val) || halves[0]==val);
    iterator i(this,range,false);
    while ( i.pos!=range && (i.deep() || !i.solid()) )
        i.towards(range);
    return i.solid() && (is_mixed(val) || *i==val);
}


void    binmap_t::map16 (uint16_t* target, bin64_t range) {
    iterator lead(this,range,true);
    if (!lead.deep()) {
        *target = *lead;
        return;
    }
    lead.left();
    lead.left();
    lead.left();
    lead.left();
    uint16_t shift = 1;
    for(int i=0; i<16; i++) {
        if (!lead.deep() && *lead==FILLED)
            *target |= shift;
        shift<<=1;
        lead.next(false,false,range.layer()-4);
    }
}


void    binmap_t::to_coarse_bitmap (uint16_t* bits, bin64_t range, uint8_t height) {
    //assert(range.layer()-height>=4);
    int height16 = range.layer()-height-4;
    int wordwidth = height16 > 0 ? (1 << height16) : 1;
    int offset = height16 > 0 ? (range.offset() << height16) : 
                                (range.offset() >> -height16); 
    for(int i=0; i<wordwidth; i++) 
        map16(bits+i,bin64_t(height+4,offset+i));
}


binheap::binheap() {
    size_ = 32;
    heap_ = (bin64_t*) malloc(size_*sizeof(bin64_t));
    filled_ = 0;
}

bool bincomp (const bin64_t& a, const bin64_t& b) {
    register uint64_t ab = a.base_offset(), bb = b.base_offset();
    if (ab==bb)
        return a.tail_bit() < b.tail_bit();
    else
        return ab > bb;
}

bool bincomp_rev (const bin64_t& a, const bin64_t& b) {
    register uint64_t ab = a.base_offset(), bb = b.base_offset();
    if (ab==bb)
        return a.tail_bit() > b.tail_bit();
    else
        return ab < bb;
}

bin64_t binheap::pop() {
    if (!filled_)
        return bin64_t::NONE;
    bin64_t ret = heap_[0];
    std::pop_heap(heap_, heap_+filled_--,bincomp);
    while (filled_ && heap_[0].within(ret))
        std::pop_heap(heap_, heap_+filled_--,bincomp);
    return ret;
}

void    binheap::extend() {
    std::sort(heap_,heap_+filled_,bincomp_rev);
    int solid = 0;
    for(int i=1; i<filled_; i++)
        if (!heap_[i].within(heap_[solid]))
            heap_[++solid] = heap_[i];
    filled_ = solid+1;
    if (2*filled_>size_) {
        size_ <<= 1;
        heap_ = (bin64_t*) realloc(heap_,size_*sizeof(bin64_t));
    }
}

void    binheap::push(bin64_t val) {
    if (filled_==size_)
        extend();
    heap_[filled_++] = val;
    std::push_heap(heap_, heap_+filled_,bincomp);
}

binheap::~binheap() {
    free(heap_);
}

