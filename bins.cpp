/*
 *  sbit.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 4/1/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "bins.h"
#include <string.h>

// make it work piece by piece

const uint8_t	bins::SPLIT[16] = 
{0, 3, 12, 15, 48, 51, 60, 63, 192, 195, 204, 207, 240, 243, 252, 255};
const uint8_t    bins::JOIN[16] =
{0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15};
const int bins::NOJOIN = 0x10000;


void bins::extend () {
    uint16_t nblocks = blocks_allocated ? (blocks_allocated<<1) : 1;
    size_t had_bytes = blocks_allocated<<6;
    size_t need_bytes = nblocks<<6;
    cells = (uint32_t*) realloc(cells,need_bytes);
    memset(((char*)cells)+had_bytes,0,need_bytes-had_bytes);
    for(int b=blocks_allocated; b<nblocks; b++)
        cells[(b<<4)|0xf] = 0x55555555; // cells are free
    blocks_allocated = nblocks;
}

bins::bins() :  height(4), blocks_allocated(0), cells(NULL), 
                ap(0), cells_allocated(0) {
    extend();
    assert(!alloc_cell());
}

bins::bins (const bins& b) : height(b.height), ap(b.ap),
blocks_allocated(b.blocks_allocated), cells_allocated(b.cells_allocated) {
    size_t memsz = blocks_allocated*16*32;
    cells = (uint32_t*) malloc(memsz);
    memcpy(cells,b.cells,memsz);
}

void bins::dump (const char* note) {
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

uint32_t bins::split16to32(uint16_t halfval) {
    uint32_t nval = 0;
    for(int i=0; i<4; i++) {
        nval >>= 8;
        nval |= (SPLIT[halfval&0xf])<<24;
        halfval >>= 4;
    }
    return nval;
}


int bins::join32to16(uint32_t cval) {
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


void        bins::split (uint32_t half) {
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


bool        bins::join (uint32_t half) {
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

void    bins::free_cell (uint16_t cell) {
    cells[cell] = 0;
    int left = cell<<1, right=left+1;
    mark(left);
    unmark(right);
    if (ap==cell+1)
        ap--;
    cells_allocated--;
}

/** Get a free cell. */
uint16_t    bins::alloc_cell () {
    uint16_t ap1 = ap;
    cells_allocated++;
    for(; ap<(blocks_allocated<<4); ap++) {
        if ((ap&0xf)==0xf)
            continue;
        if (!cells[ap] && deep(ap<<1)) {
            unmark(ap<<1);
            return ap++;
        }
    }
    if (ap1)
        ap = 0;
    else
        extend();
    cells_allocated--;
    return alloc_cell();
}


bin64_t iterator::next (bool need_solid) {
    assert(!deep());
    while (pos.is_right())
        parent();
    //parent();
    //if (need_solid ? !solid() : deep())
    //    right();
    sibling();
    while (need_solid ? !solid() : deep())
        left();
    return pos;
}


iterator::iterator(bins* host_, bin64_t start, bool split) { 
    host = host_;
    half = 0;
    for(int i=0; i<64; i++)
        history[i] = 1;
    pos = bin64_t(host->height,0);
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
    history[pos.layer()] = half; // FIXME
    pos = pos.to(right);
    half = (host->halves[half]<<1) + right;
    //host->dump("/\\ ");
}


void bins::extend_range () {
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
        history[pos.layer()+1] = 0;
    }
    pos = pos.parent();
    half = history[pos.layer()];
    host->join(half);
    //host->dump("| ");
}


bin64_t bins::find (const bin64_t range, const uint8_t layer, fill_t seek) {
    iterator i(this,range,true);
    fill_t stop = seek==EMPTY ? FILLED : EMPTY;
    while (true) {
        while ( i.bin().layer()>layer && (i.deep() || *i!=stop) )
            i.left();
        if (i.bin().layer()==layer && !i.deep() && *i==seek)
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


uint16_t bins::get (bin64_t bin) {
    iterator i(this,bin,true);
    //while ( i.pos!=bin && 
    //        (i.deep() || (*i!=BIN_FULL && *i!=BIN_EMPTY)) )
    //    i.towards(bin);
    //printf("at %i ",i.half);
    //dump("get made");
    return *i; // deep cell is never 0xffff or 0x0000
}


void bins::set (bin64_t bin, fill_t val) {
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


uint64_t*   bins::get_stripes (int& count) {
    int size = 32;
    uint64_t *stripes = (uint64_t*) malloc(32*8);
    count = 0;
    uint16_t cur = bins::EMPTY;
    stripes[count++] = 0;
    iterator i(this,0,false);
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

        i.next(true);

    }

    if ( !(count&1) )
        stripes[count++] = i.bin().base_offset();
    
    return stripes;
}


void    bins::remove (bins& b) {
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
