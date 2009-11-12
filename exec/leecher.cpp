/*
 *  leecher.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 11/3/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "p2tp.h"
#include <time.h>


using namespace p2tp;


/** P2TP downloader. Params: root hash, filename, tracker ip/port, own ip/port */
int main (int argn, char** args) {

    srand(time(NULL));

    if (argn<4) {
        fprintf(stderr,"parameters: root_hash filename tracker_ip:port [own_ip:port]\n");
        return -1;
    }
    Sha1Hash root_hash(true,args[1]);
    if (root_hash==Sha1Hash::ZERO) {
        fprintf(stderr,"Sha1 hash format: hex, 40 symbols\n");
        return -2;
    }

    p2tp::LibraryInit();

    char* filename = args[2];

    Address tracker(args[3]), bindaddr;

    if (tracker==Address()) {
        fprintf(stderr,"Tracker address format: [1.2.3.4:]12345\n");
        return -2;
    }
    if (argn>=5)
        bindaddr = Address(args[4]);
    else
        bindaddr = Address((uint32_t)INADDR_ANY,rand()%10000+7000);

    assert(0<p2tp::Listen(bindaddr));

	p2tp::SetTracker(tracker);

	int file = p2tp::Open(filename,root_hash);
    printf("Downloading %s\n",root_hash.hex().c_str());

    while (!p2tp::IsComplete(file)) {
	    p2tp::Loop(TINT_SEC);
        printf("done %lli of %lli (seq %lli) %lli dgram %lli bytes up, %lli dgram %lli bytes down\n",
               p2tp::Complete(file), p2tp::Size(file), p2tp::SeqComplete(file),
               Datagram::dgrams_up, Datagram::bytes_up,
               Datagram::dgrams_down, Datagram::bytes_down );
    }

	p2tp::Close(file);

	p2tp::Shutdown();

}
