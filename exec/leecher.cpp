/*
 *  leecher.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 11/3/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "p2tp.h"


using namespace p2tp;


int main (int argn, char** args) {
    
    assert(0<p2tp::Listen(Datagram::Address(INADDR_ANY,7002)));
	p2tp::SetTracker(Datagram::Address("130.161.211.198",7001));
    unlink("doc/sofi-copy.jpg");
	int file = p2tp::Open("doc/sofi-copy.jpg",
                          Sha1Hash(true,"282a863d5567695161721686a59f0c667250a35d"));
    
    while (!p2tp::Complete(file)) {
	    p2tp::Loop(TINT_SEC/10);
        printf("%lli dgram %lli bytes up, %lli dgram %lli bytes down\n",
               Datagram::dgrams_up, Datagram::bytes_up,
               Datagram::dgrams_down, Datagram::bytes_down );
    }
    
	p2tp::Close(file);
    
	p2tp::Shutdown();
    
}
