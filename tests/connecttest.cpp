/*
 *  connecttest.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/19/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include <gtest/gtest.h>
#include <glog/logging.h>
#include "p2tp.h"

using namespace p2tp;


TEST(P2TP,CwndTest) {

    srand ( time(NULL) );
    
    unlink("doc/sofi-copy.jpg");
    struct stat st;
	ASSERT_EQ(0,stat("doc/sofi.jpg", &st));
    int size = st.st_size;//, sizek = (st.st_size>>10) + (st.st_size%1024?1:0) ;
    
    int sock1 = p2tp::Listen(7001);
	ASSERT_TRUE(sock1>=0);
	
	int file = p2tp::Open("doc/sofi.jpg");
    FileTransfer* fileobj = FileTransfer::file(file);
    FileTransfer::instance++;
    
    p2tp::SetTracker(Address("127.0.0.1",7001));
    
	int copy = p2tp::Open("doc/sofi-copy.jpg",fileobj->root_hash());
  
	p2tp::Loop(TINT_SEC);
    
    int count = 0;
    while (p2tp::SeqComplete(copy)!=size && count++<600)
        p2tp::Loop(TINT_SEC);
    ASSERT_EQ(size,p2tp::SeqComplete(copy));
    
	p2tp::Close(file);
	p2tp::Close(copy);

	p2tp::Shutdown(sock1);

}


int main (int argc, char** argv) {
	
	google::InitGoogleLogging(argv[0]);
	testing::InitGoogleTest(&argc, argv);
	int ret = RUN_ALL_TESTS();
	return ret;
	
}
