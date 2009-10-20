/*
 *  readwrite.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/19/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include <gtest/gtest.h>
#include "p2tp.h"

TEST(P2TP, ConnectTest) {
	P2File("");
	p2tp_init(7001);
	
	int tf = p2tp_open("test_file",NULL);
	int tb = p2tp_open("test_file_copy",p2tp_file_info(tf)->hash_data);
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7001);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	p2tp_add_peer(tb,addr,0); // TRICK: will open a channel to the first file
	p2tp_loop(P2TP::now()+TINT1SEC/10);

	while (count=copy.read(bytes)) {
		read(orig,bytes2,count);
		ASSERT_EQ ( 0, memcmp(bytes,bytes2,count) );
	}
	
	p2tp_close(tb);
	p2tp_close(tf);
	
}

int main (int argc, char** argv) {
	P2TP::init();
	
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
	
}
