/*
 *  hasher.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 11/8/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include <stdio.h>
#include "hashtree.h"


int main (int argn, char** args) {
    
    if (argn<2) {
        fprintf(stderr,"Usage: %s file_name\n",args[0]);
        return 1;
    }

    p2tp::HashTree* ht = new p2tp::HashTree(args[1]);
    
    printf("SHA1 Merkle tree root hash: %s\n",ht->root_hash().hex().c_str());
    
}

