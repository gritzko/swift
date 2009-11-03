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


int main (int argn, char** args) {
    
    assert(0<p2tp::Listen(7001));
	
	int file = p2tp::Open("doc/sofi.jpg");
    
    while (true) {
	    p2tp::Loop(TINT_SEC);
        printf("%lli dgram %lli bytes up, %lli dgram %lli bytes down\n",
            Datagram::dgrams_up, Datagram::bytes_up,
            Datagram::dgrams_down, Datagram::bytes_down );
    }
    
	p2tp::Close(file);

	p2tp::Shutdown();

}

