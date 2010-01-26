/*
 *  mmap_storer.cpp
 *  swift
 *
 *  Created by Victor Grishchenko on 10/7/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "swift.h"

class MMappedStorer : public DataStorer {
public:
    
    DataStorer (Sha1Hash id, size_t size) {
    }
    
    virtual size_t    ReadData (bin64_t pos,uint8_t** buf) {
    }
    
    virtual size_t    WriteData (bin64_t pos, uint8_t* buf, size_t len) {
    }
    
    virtual Sha1Hash* LoadHashes();
    
};