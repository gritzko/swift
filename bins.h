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
#include "bin64.h"

class bins64 {

    private:

        uint32_t    *bits;
        uint32_t    alloc_block;

    protected:

        bool        join(uint32_t half) {
            uint32_t cellno = bits[half]>>(half&1?16:0);

            if (deep(left) || deep(right)) // some half is deep
                return false;
            uint8_t b1=JOIN[cell&0xf],
                    b2=JOIN[(cell>>8)&0xf],
                    b3=JOIN[(cell>>16)&0xf],
                    b4=JOIN[cell>>24];
            if (b1==0xff || b2==0xff || b3==0xff || b4==0xff)
                return false;
            bits[half] = b1 | (b2<<4) | (b3<<8) | (b4<<12) ;
            (*parentdeepcell) ^= 1<<(halfno&32);
            (*childdeepcell) &= 0xffff>>1; // clean the full bit
        }

        bool        split(uint32_t half) {
            if (deep(half))
                return false;
            uint32_t cell = alloc_cell(), left=cell<<1, right=left+1;
            bits[half|0xf] |= 1<<(half&0xf);
            bits[left] = SPLIT[bits[half]>>8];
            bits[right] = SPLIT[bits[half&0xff]];
            bits[half] = cell;
            return true;
        }

        uint32_t    alloc_cell () {
            do{
                for(int block=alloc_block; bits[block]&(1<<32); block+=32);
                for(int off=0; bits[block+off]==0 && off<31; off++);
                if (off==31) 
                    bits[block] |= 1<<32;
                if (block&(1<<31)) {
                    bits = realloc(bits,block*2);
                    memset();
                }
            } while (off==31);
            alloc_block = block;
            return block+off;
        }

    public:

        class iterator {
            bins64_t    *host;
            uint32_t    back[32];
            uint32_t    half;
            bin64_t     top;
            bin64_t     focus;
            bin16_t     mask;
            public:
            void left();
            void right();
            void parent();
            bin64_t next();
            bool defined();
            uint16_t& operator* ();
        };
        friend class iterator;

        bool get (uint64_t bin);

        void set (uint64_t bin);

        bin64_t find (bin64_t range, int layer);

        // TODO: bitwise operators

};
