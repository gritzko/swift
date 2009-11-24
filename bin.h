/*
 *  bin.h
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifndef BIN_H
#define BIN_H
#include <assert.h>
#ifdef _MSC_VER
    // To avoid complaints about std::max. Appears to work in VS2008
    #undef min
    #undef max
    #include "compat/stdint.h"
#else
    #include <stdint.h>
#endif
#include <deque>

struct bin {
    uint32_t    b;

    static bin    NONE;
    static bin    ALL;
    static uint8_t BC[256];
    static uint8_t T0[256];

    bin() : b(0) {}
    bin(const bin& b_) : b(b_.b) {}
    bin(uint32_t b_) : b(b_) {}

    bin(uint8_t layer_, uint32_t offset) {
        b = lenpeak((offset+1)<<layer_);
        b -= layer() - layer_;
    }

    static void    init ();

    static uint8_t tailzeros (uint32_t i) {
        uint8_t ret = 0;
        if ( (i&0xffff)==0 )
            ret = 16, i>>=16;
        if ( (i&0xff)==0 )
            ret +=8, i>>=8;
        return ret+T0[i&0xff];
    }

    static uint8_t bitcount (uint32_t i) {
        //uint8_t* p = (uint8_t*) &i;
        //return BC[p[0]] + BC[p[1]] + BC[p[2]] + BC[p[3]];
        return  BC[i&0xff] +
                BC[(i>>8)&0xff] +
                BC[(i>>16)&0xff] +
                BC[i>>24];
    }

    static uint32_t    blackout (uint32_t i) {
        return i|=(i|=(i|=(i|=(i|=i>>1)>>2)>>4)>>8)>>16;
    }

    static uint32_t highbit (uint32_t i) {
        return (blackout(i)+1)>>1;
    }

    static bool all1 (uint32_t a) {
        return !(a&(a+1));
    }

    static bin  lenpeak (uint32_t length) {
        return (length<<1) - bitcount(length);
    }

    static uint8_t lenlayer (uint32_t len) {
        return tailzeros(len);
    }

    static bin  layermass (uint8_t layer) {
        return (2<<layer)-1;
    }

    static uint32_t lastbiton (uint32_t i) {
        return (~i+1)&i;
    }

    typedef std::deque<bin> vec;
    static vec peaks (uint32_t len);

    static void order (vec* v);

    operator uint32_t() const {    return b;  }

    bin            operator ++ () { return b++; }
    bin            operator -- () { return b--; }
    bin            operator ++ (int) { return ++b; }
    bin            operator -- (int) { return --b; }

    uint32_t    mlat() const {
        return 0;
    }

    bin            left() const {
        return bin(b-(mass()>>1)-1);
    }

    bin            right() const {
        return bin(b-1);
    }

    bin            right_foot() const {
        return bin(b-layer());
    }

    bin            left_foot() const {
        return bin(b-mass()+1);
    }

    uint32_t    length() const {
        //assert(*this<=ALL);
        uint32_t apx = (b>>1) + 16; //if (b<=ALL-32) apx = ALL>>1;
        uint32_t next = apx-8;
        next = apx = lenpeak(next)>=b ? next : apx;
        next -= 4;
        next = apx = lenpeak(next)>=b ? next : apx;
        next -= 2;
        next = apx = lenpeak(next)>=b ? next : apx;
        next -= 1;
        next = apx = lenpeak(next)>=b ? next : apx;
        return apx;
    }

    uint32_t    mass() const {
        return layermass(layer());
    }

    uint8_t        layer() const {
        uint32_t len = length();
        uint8_t topeak = lenpeak(len) - b;
        return lenlayer(len) - topeak;
    }

    uint32_t    width () const {
        return 1<<layer();
    }

    bin            peak() const {
        return lenpeak(length());
    }

    bin            divide (uint8_t ls) const {
        uint32_t newlen = ((length()-1)>>ls) +1;
        uint8_t newlr = std::max(0,layer()-ls);
        return lenpeak(newlen) - lenlayer(newlen) + newlr;
    }

    uint32_t    offset () const {
        return length() - width();
    }

    bin            modulo (uint8_t ls) const {
        if (layer()>=ls)
            return layermass(ls);
        bin blockleft = lenpeak(((length()-1) & ~((1<<ls)-1)) + 1);
        return b - blockleft + 1;
    }

    bin            multiply (uint8_t ls) const {
        return b + length()*(layermass(ls)-1);
    }

    bool        contains (bin c) const {
        return c.b<=b && c.b>b-mass();
    }

    bin            commonParent (bin other) const {
        uint8_t maxlayer = std::max(layer(),other.layer());
        uint32_t myoff = offset()>>maxlayer, othoff = other.offset()>>maxlayer;
        uint32_t diff = blackout(myoff^othoff);
        uint8_t toshift = bitcount(diff);
        return bin(maxlayer+toshift,myoff>>toshift);
    }

    bin            child (bin dir) const {
        return left().contains(dir) ? left() : right();
    }

    bin            parent (uint8_t g=1) const {
        uint32_t l = length();
        uint8_t h2b = layer()+g;
        uint32_t pbit = 1<<h2b;
        uint32_t l2b = l & ~(pbit-1);
        if (l2b!=l)
            l2b += pbit;
        return lenpeak(l2b) - lenlayer(l2b) + h2b;
        //length()==bin(b+1).length() ? b+1 : b+mass()+1;
    }

    bool        is_right () const {
        return this->parent()==b+1;
    }

    bool        is_left () const {
        return !is_right();
    }

    bin            sibling () const {
        return is_left() ? bin(b+mass()) : bin(b-mass());
    }

    bin            scoped (bin top, uint8_t height) const {
        assert(layer()<=top.layer());    // TERRIBLE
        assert(top.layer()>=height);
        uint8_t rel_layer;
        if (layer()+height>=top.layer())
            rel_layer = layer()+height-top.layer();
        else
            rel_layer = 0;//top.layer() - height;
        uint32_t rel_offset = (offset()-top.offset()) >> (top.layer()-height+rel_layer);
        return bin(rel_layer,rel_offset);
    }

    bin            unscoped (bin top, uint8_t height) const {
        uint32_t undermass = layermass(top.layer()-height);
        uint32_t pad = (1<<height) - length();
        uint32_t peak = (1<<(height+1))-1;
        return top - (peak-this->b) + pad - undermass*pad;
    }

} ;


uint8_t    bitcount (uint32_t num);

/*bin l=b>a.b?a.b:b, g=b>a.b?b:a.b;
 while (!g.contains(l))
 g = g.parent();
 return g;*/

#endif
//20 mln ops per second
