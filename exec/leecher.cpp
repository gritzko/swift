/*
 *  leecher.cpp
 *  swift
 *
 *  Created by Victor Grishchenko on 11/3/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "swift.h"
#include <time.h>


using namespace swift;


/** swift downloader. Params: root hash, filename, tracker ip/port, own ip/port */
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

    swift::LibraryInit();

    char* filename = args[2];

    Address tracker(args[3]), bindaddr;

    if (tracker==Address()) {
        fprintf(stderr,"Tracker address format: [1.2.3.4:]12345, not %s\n",args[3]);
        return -2;
    }
    if (argn>=5)
        bindaddr = Address(args[4]);
    else
        bindaddr = Address((uint32_t)INADDR_ANY,rand()%10000+7000);

    if (swift::Listen(bindaddr)<=0) {
        fprintf(stderr,"Cannot listen on %s\n",bindaddr.str());
        return -3;
    }

	swift::SetTracker(tracker);

	int file = swift::Open(filename,root_hash);
    printf("Downloading %s\n",root_hash.hex().c_str());

    tint start = NOW;

    while (true){ //!swift::IsComplete(file)){// && NOW-start<TINT_SEC*60) {
	    swift::Loop(TINT_SEC);
        eprintf("%s %lli of %lli (seq %lli) %lli dgram %lli bytes up, %lli dgram %lli bytes down\n",
               swift::IsComplete(file) ? "DONE" : "done",
               swift::Complete(file), swift::Size(file), swift::SeqComplete(file),
               Datagram::dgrams_up, Datagram::bytes_up,
               Datagram::dgrams_down, Datagram::bytes_down );
    }

    bool complete = swift::IsComplete(file);

	swift::Close(file);

	swift::Shutdown();

    return !complete;
}
