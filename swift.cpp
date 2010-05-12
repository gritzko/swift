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
#include "compat.h"
#include "swift.h"

using namespace swift;

#define quit(...) {fprintf(stderr,__VA_ARGS__); exit(1); }
SOCKET InstallHTTPGateway (Address addr);


int main (int argc, char** argv) {
    
    static struct option long_options[] =
    {
        {"hash",    required_argument, 0, 'h'},
        {"file",    required_argument, 0, 'f'},
        {"daemon",  no_argument, 0, 'd'},
        {"listen",  required_argument, 0, 'l'},
        {"tracker", required_argument, 0, 't'},
        {"debug",   no_argument, 0, 'D'},
        {"progress",no_argument, 0, 'p'},
        {"http",    optional_argument, 0, 'g'},
        {"wait",    optional_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    Sha1Hash root_hash;
    char* filename = 0;
    bool daemonize = false, report_progress = false;
    Address bindaddr;
    Address tracker;
    Address http_gw;
    tint wait_time = -1;
    
    LibraryInit();
    
    int c;
    while ( -1 != (c = getopt_long (argc, argv, ":h:f:dl:t:Dpg::w::", long_options, 0)) ) {
        
        switch (c) {
            case 'h':
                if (strlen(optarg)!=40)
                    quit("SHA1 hash must be 40 hex symbols\n");
                root_hash = Sha1Hash(true,optarg); // FIXME ambiguity
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
                if (wait_time==-1)
                    wait_time = TINT_NEVER; // seed
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
            case 'g':
                http_gw = optarg ? Address(optarg) : Address(Address::LOCALHOST,8080);
                if (wait_time==-1)
                    wait_time = TINT_NEVER; // seed
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
    

    if (bindaddr!=Address()) { // seeding
        if (Listen(bindaddr)<=0)
            quit("cant listen to %s\n",bindaddr.str())
    } else if (tracker!=Address() || http_gw!=Address()) { // leeching
        for (int i=0; i<=10; i++) {
            bindaddr = Address((uint32_t)INADDR_ANY,0);
            if (Listen(bindaddr)>0)
                break;
            if (i==10)
                quit("cant listen on %s\n",bindaddr.str());
        }
    }
    
    if (tracker!=Address())
        SetTracker(tracker);

    if (http_gw!=Address())
        InstallHTTPGateway(http_gw);

    if (root_hash!=Sha1Hash::ZERO && !filename)
        filename = strdup(root_hash.hex().c_str());

    int file = -1;
    if (filename) {
        file = Open(filename,root_hash);
        if (file<=0)
            quit("cannot open file %s",filename);
        printf("Root hash: %s\n", RootMerkleHash(file).hex().c_str());
    }

    if (bindaddr==Address() && file==-1) {
        fprintf(stderr,"Usage:\n");
        fprintf(stderr,"  -h, --hash\troot Merkle hash for the transmission\n");
        fprintf(stderr,"  -f, --file\tname of file to use (root hash by default)\n");
        fprintf(stderr,"  -l, --listen\t[ip:|host:]port to listen to (default: random)\n");
        fprintf(stderr,"  -t, --tracker\t[ip:|host:]port of the tracker (default: none)\n");
        fprintf(stderr,"  -D, --debug\tfile name for debugging logs (default: stdout)\n");
        fprintf(stderr,"  -p, --progress\treport transfer progress\n");
        fprintf(stderr,"  -g, --http\t[ip:|host:]port to bind HTTP gateway to (default localhost:8080)\n");
        fprintf(stderr,"  -w, --wait\tlimit running time, e.g. 1[DHMs] (default: infinite with -l, -g)\n");
    }

    tint start_time = NOW;
    
    while ( bindaddr!=Address() &&
            ( ( file>=0 && !IsComplete(file) ) ||
              ( start_time+wait_time > NOW ) )   ) {
        swift::Loop(TINT_SEC);
        if (report_progress && file>=0) {
            fprintf(stderr,
                    "%s %lli of %lli (seq %lli) %lli dgram %lli bytes up, "\
                    "%lli dgram %lli bytes down\n",
                IsComplete(file) ? "DONE" : "done",
                Complete(file), Size(file), SeqComplete(file),
                Datagram::dgrams_up, Datagram::bytes_up,
                Datagram::dgrams_down, Datagram::bytes_down );
        }
    }
    
    if (file!=-1)
        Close(file);
    
    if (Channel::debug_file)
        fclose(Channel::debug_file);
    
    swift::Shutdown();
    
    return 0;
    
}

