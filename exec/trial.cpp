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
#include <string>
#include "compat/util.h"
#ifndef _WIN32
#include <netdb.h>
#endif


using namespace swift;


/** P2TP downloader. Params: root hash, filename, tracker ip/port, own ip/port */
int main (int argn, char** args) {
    THIS IS NOT TRIAL BRANCH
    srand(time(NULL));
    Sha1Hash root_hash(true,"32e5d9d2d8c0f6073e2820cf47b15b58c2e42a23");
    swift::LibraryInit();

    // Arno: use tempdir
    std::string tmpdir = gettmpdir();
    std::string sfn = tmpdir+"team.jpg";
    std::string hfn = tmpdir+"team.jpg.mhash";
    const char* filename = strdup(sfn.c_str());
    unlink(filename);
    unlink(hfn.c_str());

    Address tracker("victor.p2p-next.org:12345"),
            bindaddr((uint32_t)INADDR_ANY,10000),
            fallback("victor2.p2p-next.org:12345");

    if (0>swift::Listen(bindaddr)) {
        print_error("cannot bind");
        return 1;
    }
	swift::SetTracker(tracker);
	int file = swift::Open(filename,root_hash);
    printf("Downloading %s\n",root_hash.hex().c_str());
    int count = 400;
    while (!swift::IsComplete(file) && count-->0) {
	    swift::Loop(TINT_SEC/10);
        if (count==100) 
            FileTransfer::file(file)->OnPexIn(fallback);
        printf("done %lli of %lli (seq %lli) %lli dgram %lli bytes up, %lli dgram %lli bytes down\n",
               swift::Complete(file), swift::Size(file), swift::SeqComplete(file),
               Datagram::dgrams_up, Datagram::bytes_up,
               Datagram::dgrams_down, Datagram::bytes_down );
    }
    int ret = !swift::IsComplete(file);

	swift::Close(file);
	swift::Shutdown();

    return ret;

}
