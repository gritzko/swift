/*
 *  connecttest.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/19/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "p2tp.h"


using namespace p2tp;


/** P2TP seeder. Params: filename, own ip/port, tracker ip/port */
int main (int argn, char** args) {
    
    if (argn<3) {
        fprintf(stderr,"parameters: filename own_ip/port [tracker_ip/port]\n");
        return -1;
    }
    
    char* filename = args[1];
    
    if (argn>=4) {
        Address tracker(args[3]);
        p2tp::SetTracker(tracker);
    }
    Address bindaddr(args[2]);
    
    if (bindaddr==Address()) {
        fprintf(stderr,"Bind address format: [1.2.3.4:]12345\n");
        return -2;
    }
    
    assert(0<p2tp::Listen(bindaddr));
    printf("seeder bound to %s\n",bindaddr.str().c_str());

    
	int file = p2tp::Open(filename);
    printf("seeding %s %s\n",filename,RootMerkleHash(file).hex().c_str());
    
    while (true) {
	    p2tp::Loop(TINT_SEC*60);
        printf("%lli dgram %lli bytes up, %lli dgram %lli bytes down\n",
               Datagram::dgrams_up, Datagram::bytes_up,
               Datagram::dgrams_down, Datagram::bytes_down );
    }
    
	p2tp::Close(file);
    
	p2tp::Shutdown();
    
}
