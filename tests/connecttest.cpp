/*
 *  connecttest.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/19/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include <gtest/gtest.h>
//#include <glog/logging.h>
#include "swift.h"
#include <time.h>


using namespace swift;


TEST(Connection,CwndTest) {

    srand ( time(NULL) );

    unlink("doc/sofi-copy.jpg");
    struct stat st;
	ASSERT_EQ(0,stat("doc/sofi.jpg", &st));
    int size = st.st_size;//, sizek = (st.st_size>>10) + (st.st_size%1024?1:0) ;
    Channel::SELF_CONN_OK = true;

    int sock1 = swift::Listen(7001);
	ASSERT_TRUE(sock1>=0);

	int file = swift::Open("doc/sofi.jpg");
    FileTransfer* fileobj = FileTransfer::file(file);
    //FileTransfer::instance++;

    swift::SetTracker(Address("127.0.0.1",7001));

	int copy = swift::Open("doc/sofi-copy.jpg",fileobj->root_hash());

	swift::Loop(TINT_SEC);

    int count = 0;
    while (swift::SeqComplete(copy)!=size && count++<600)
        swift::Loop(TINT_SEC);
    ASSERT_EQ(size,swift::SeqComplete(copy));

	swift::Close(file);
	swift::Close(copy);

	swift::Shutdown(sock1);

}


int main (int argc, char** argv) {

	swift::LibraryInit();
	testing::InitGoogleTest(&argc, argv);
    Channel::debug_file = stdout;
	int ret = RUN_ALL_TESTS();
	return ret;

}
