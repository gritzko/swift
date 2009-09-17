/*
 *  rarest1st.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */


char*	Rarest1st::on_event (P2Channel& ch) {
	if (!ch.peer_has)
		return "nothing is known - cannot choose";
	feye i_need (ch.file->have,ch.peer_has->focus);
	i_need.invert();
	i_need &= ch.peer_has;
	if (i_need.clean())
		return "the peer has nothing we don't have";
	feye ladder[20];
	for(int i=P2TP_CHANNELS.begin(); i!=P2TP_CHANNELS.end(); i++) {
		p2tp_channel* c = *i;
		if (!c || !c->peer_has)
			continue;
		feye x = feye_and(c->peer_has,i_need);
		for(int j=0; j<20 && !x.clean(); j++) {
			feye xx(x);
			x &= ladder[j];
			ladder[j] ^= xx;
		}
	}
	feye not_rare (ch.peer_has->focus);
	for(int i=20-1; i>0; i--) {
		not_rare |= ladder[i];
		ladder[i-1] -= not_rare;
	}
	int pickfrom = 0;
	while (ladder[pickfrom].clean() && pickfrom<20)
		pickfrom++;
	assert(pickfrom<20);
	mlat spot = ladder[pickfrom].randomBit();
	ch->i_ackd.refocus(spot);
	return NULL;
}
