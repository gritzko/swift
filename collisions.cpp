/*
 *  collisions.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/15/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include "bin.h"
#include "sbit.h"
#include <deque>
#include <cmath>

using namespace std;

#define NUMPEERS 40
#define QUEUE_LEN 32
#define WIDTH	20
#define TOP (bin(WIDTH,0))

sbit	bigpic; // common knowledge

struct Peer {
	deque<bin>	packets; // packets in flight & unacknowledged
	feye		map;
	int peerno;
	bin			focus;
	Peer () : map(bigpic,bin(0,rand()%(1<<WIDTH))) {
		focus = map.focus;
	}
	void jump () {
		bin oldfoc = focus;
		
		map = feye(bigpic,focus);
		for(int i=0; i<packets.size(); i++)
			map|=packets[i];
		
		while (map.get(focus) && focus<TOP)
			focus = focus.parent();
		if (focus==TOP) {
			printf("DONE\n");
			packets.push_back(0);
			return;
		}
		//if (focus!=bin(WIDTH,0))
		//	focus = focus.parent();
		while (focus.layer()) {
			bin left = focus.left(), right = focus.right();
			bool lfull = map.get(left);
			bool rfull = map.get(right);
			if (lfull)
				focus = right;
			else if (rfull)
				focus = left;
			else
				focus = rand()%2 ? left : right;
		}
		if (map.focus.commonParent(focus).layer()>6)
			map.refocus(focus);
		if (labs(map.focus-focus)>=129)
			printf("bred!\n");
		map |= focus;
		// sanity check
		if (bigpic.get(focus))
			printf("zhopa: peer %i redid %x after jumped %i\n",
				   peerno,focus.offset(),oldfoc.commonParent(focus).layer());
		assert(focus.layer()==0 && focus<TOP);
		packets.push_back(focus);
	}
	void hint (bin newfoc) {
		printf("peer %i hinted at %x\n",
			   peerno,newfoc.offset());
		focus = newfoc;
		//map.refocus(newfoc);	// preserve recent sends if possible
		//map |= feye(bigpic,newfoc); // update with the big picture
		map = feye(bigpic,newfoc);
	}
	void ack (bin packet) {
		feye a(bigpic,packet);
		a.refocus(map.focus);
		map |= a;
	}
};


bin random_descend() {
	bin ret = bin(WIDTH,0);
	while (ret.layer()) {
		bin left = ret.left(), right = ret.right();
		bool lfull = bigpic.get(left);
		bool rfull = bigpic.get(right);
		if (lfull)
			ret = right;
		else if (rfull)
			ret = left;
		else
			ret = rand()%2 ? left : right;
	}
	return ret;
}


int	main (int argc, char** args) {
	bin::init();
	sbit::init();
	freopen( "log", "w", stdout );
	Peer peers[NUMPEERS];
	int numpack = 0;
	int coll_layer[32] = {	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
							0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0	};
	for(int i=0; i<NUMPEERS; i++) 
		peers[i].peerno = i;
	while ( ! bigpic.get(bin(WIDTH,0)) ) {
		for(int i=0; i<NUMPEERS; i++) 
			peers[i].jump();
		for(int i=0; i<NUMPEERS; i++)
			if (peers[i].packets.size()>=QUEUE_LEN) { // round trip, acks reach senders
				bin packet = peers[i].packets.front();
				peers[i].packets.pop_front();
				if (packet==0) continue;
				bool collision = bigpic.get(packet);
				bin round = bigpic.set(packet);
				printf("peer %i arrived %x filled %i coll %i\n",
					   i,packet.offset(),round.layer(),collision);
				for(int j=0; j<NUMPEERS; j++)
					peers[j].map |= round;
				peers[i].ack(packet);
				numpack++;
				if (collision) {
					peers[i].hint(random_descend());
					coll_layer[round.layer()]++;
				}
				/*{	// update with the big picture
					feye update(bigpic,packet); // current focus is unknown
					update.refocus(peers[i].focus);
					peers[i].map |= update;
				}*/
			}
	}
	printf("%i useful, %i total plus the tail\n",1<<WIDTH,numpack);
	for(int l=0;l<32; l++)
		printf("%i ",coll_layer[l]);
	printf("\n");
}

