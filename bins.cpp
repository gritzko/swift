/*
 *  sbit.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 4/1/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include "bins.h"

uint16_t		bins::SPLIT[256];
uint8_t			bins::JOIN[256];
uint16_t		bins::MASK1000[32];

bin64_t bins::find (bin64_t range, int layer) {
    // fall to left border
    // do
    //  if up && has 0 success
    //  if lower && range-shift-trick
    //      success
    // while (next && within range)
    // return fail
}
