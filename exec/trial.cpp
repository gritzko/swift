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
#include <string>
#include "compat/util.h"


using namespace p2tp;


/** P2TP downloader. Params: root hash, filename, tracker ip/port, own ip/port */
int main (int argn, char** args) {

    srand(time(NULL));
    Sha1Hash root_hash(true,"32e5d9d2d8c0f6073e2820cf47b15b58c2e42a23");
    p2tp::LibraryInit();

    // Arno: use tempdir
    std::string tmpdir = gettmpdir();
    std::string sfn = tmpdir+"team.jpg";
    const char* filename = sfn.c_str();


    Address tracker("130.161.211.198:10000"),
            bindaddr((uint32_t)INADDR_ANY,10000);
    if (0>p2tp::Listen(bindaddr)) {
        print_error("cannot bind");
        return 1;
    }
	p2tp::SetTracker(tracker);
	int file = p2tp::Open(filename,root_hash);
    printf("Downloading %s\n",root_hash.hex().c_str());
    int count = 100;
    while (!p2tp::IsComplete(file) && count-->0) {
	    p2tp::Loop(TINT_SEC/10);
        printf("done %lli of %lli (seq %lli) %lli dgram %lli bytes up, %lli dgram %lli bytes down\n",
               p2tp::Complete(file), p2tp::Size(file), p2tp::SeqComplete(file),
               Datagram::dgrams_up, Datagram::bytes_up,
               Datagram::dgrams_down, Datagram::bytes_down );
    }

	p2tp::Close(file);
	p2tp::Shutdown();

}
