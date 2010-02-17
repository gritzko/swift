/*
 *  swift.cpp
 *  swift the multiparty transport protocol
 *
 *  Created by Victor Grishchenko on 2/15/10.
 *  Copyright 2010 Delft University of Technology. All rights reserved.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "swift.h"

using namespace swift;

#define quit(...) {fprintf(stderr,__VA_ARGS__); exit(1); }


int main (int argc, char** argv) {
    
    static struct option long_options[] =
    {
        {"hash",    required_argument,       0, 'h'},
        {"file",    required_argument,       0, 'f'},
        {"daemon",  no_argument, 0, 'd'},
        {"listen",  required_argument, 0, 'l'},
        {"tracker", required_argument, 0, 't'},
        {"debug",   no_argument, 0, 'D'},
        {"progress",no_argument, 0, 'p'},
        {"wait",    optional_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    Sha1Hash root_hash;
    char* filename = 0;
    bool daemonize = false, report_progress = false;
    Address bindaddr;
    Address tracker;
    tint wait_time = 0;
    
    int c;
    while ( -1 != (c = getopt_long (argc, argv, ":h:f:dl:t:Dpw::", long_options, 0)) ) {
        
        switch (c) {
            case 'h':
                if (strlen(optarg)!=40)
                    quit("SHA1 hash must be 40 hex symbols\n");
                root_hash = Sha1Hash(optarg);
                if (root_hash==Sha1Hash::ZERO)
                    quit("SHA1 hash must be 40 hex symbols\n");
                break;
            case 'f':
                filename = strdup(optarg);
                break;
            case 'd':
                daemonize = true;
                break;
            case 'l':
                bindaddr = Address(optarg);
                if (bindaddr==Address())
                    quit("address must be hostname:port, ip:port or just port\n");
                break;
            case 't':
                tracker = Address(optarg);
                if (tracker==Address())
                    quit("address must be hostname:port, ip:port or just port\n");
                SetTracker(tracker);
                break;
            case 'D':
                Channel::debug_file = optarg ? fopen(optarg,"a") : stdout;
                break;
            case 'p':
                report_progress = true;
                break;
            case 'w':
                wait_time = TINT_NEVER;
                if (optarg) {
                    char unit = 'u';
                    if (sscanf(optarg,"%lli%c",&wait_time,&unit)!=2)
                        quit("time format: 1234[umsMHD], e.g. 1M = one minute\n");
                    switch (unit) {
                        case 'D': wait_time *= 24;
                        case 'H': wait_time *= 60;
                        case 'M': wait_time *= 60;
                        case 's': wait_time *= 1000;
                        case 'm': wait_time *= 1000;
                        case 'u': break;
                        default:  quit("time format: 1234[umsMHD], e.g. 1D = one day\n");
                    }
                }
                break;
        }

    }   // arguments parsed
    
    LibraryInit();
    
	int file = Open(filename,root_hash);
    // FIXME open err 
    printf("Root hash: %s\n", RootMerkleHash(file).hex().c_str());
    
    if (root_hash==Sha1Hash() && bindaddr==Address())
        exit(0);

    if (bindaddr!=Address()) { // seeding
        if (Listen(bindaddr)<=0)
            quit("cant listen to %s\n",bindaddr.str())
        if (wait_time==0)
            wait_time=TINT_NEVER;
    } else {
        int base = rand()%10000, i;
        for (i=0; i<100 && Listen(Address(INADDR_ANY,i*7+base))<=0; i++);
        if (i==100)
            quit("cant listen to a port\n");
    }
    
    
    if (tracker!=Address())
        SetTracker(tracker);
    
    tint start_time = NOW;
    tint end_time = TINT_NEVER;
    
    while (NOW<end_time+wait_time){
        if (end_time==TINT_NEVER && IsComplete(file))
            end_time = NOW;
        // and yes, I add up infinities and go away with that
        tint towait = (end_time+wait_time)-NOW;
	    Loop(std::min(TINT_SEC,towait));
        if (report_progress) {
            fprintf(stderr,
                    "%s %lli of %lli (seq %lli) %lli dgram %lli bytes up, "\
                    "%lli dgram %lli bytes down\n",
                IsComplete(file) ? "DONE" : "done",
                Complete(file), Size(file), SeqComplete(file),
                Datagram::dgrams_up, Datagram::bytes_up,
                Datagram::dgrams_down, Datagram::bytes_down );
        }
    }
    
	Close(file);
    
    if (Channel::debug_file)
        fclose(Channel::debug_file);
    
	Shutdown();
    
    return 0;
    
}

