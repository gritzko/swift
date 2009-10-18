/*
 *  connecttest.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/19/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */

#include <gtest/gtest.h>
#include <glog/logging.h>
#include "p2tp.h"

using namespace p2tp;

/*TEST(P2TP, ConnectTest) {

	uint8_t buf[1024];
	int f = open("test_file",O_RDWR|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	for(char c='a'; c<='c'; c++) {
		memset(buf,c,1024);
		write(f,buf,1024);
	}
	close(f);
	
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7001);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	int sock = p2tp::Init(7001);
	ASSERT_TRUE(0<sock);

	int file = p2tp::Open("test_file");
	p2tp::File& fileobj = * p2tp::File::file(file);
	int copy = p2tp::Open(fileobj.root_hash(),"test_file_copy");
	p2tp::File& copyobj = * p2tp::File::file(copy);
	int chan = p2tp::Connect(copy,sock,addr); // TRICK: will open a channel to the first file
	p2tp::Loop(p2tp::TINT_1SEC);
	//ASSERT_EQ(p2tp::Channel::channel(chan)->state(),p2tp::Channel::HS_DONE); FIXME: status
	ASSERT_EQ(p2tp::file_size(file),p2tp::file_size(copy));
	ASSERT_EQ(p2tp::File::DONE,copyobj.status());
	p2tp::Close(file);
	p2tp::Close(copy);
	
	p2tp::Shutdown(sock);
	
}*/


TEST(P2TP,CwndTest) {
	int f = open("big_test_file",O_RDWR|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	int size = rand()%(1<<19) + (1<<19);
    int sizek = (size>>10) + ((size&1023)?1:0);
	char* b = (char*)malloc(size);
	for(int i=0; i<size; i++) 
		b[i] = (i%1024!=1023) ? ('A' + rand()%('Z'-'A')) : ('\n');
	write(f,b,size);
	free(b);
	close(f);

	/*
	struct sockaddr_in addr1, addr2;
	addr1.sin_family = AF_INET;
	addr1.sin_port = htons(7003);
	addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  
	addr2.sin_family = AF_INET;
	addr2.sin_port = htons(7004);
	addr2.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	  */
    int sock1 = p2tp::Listen(7003);
	ASSERT_TRUE(sock1>=0);
	//ASSERT_TRUE(sock2>=0);
	
    p2tp::AddPeer(Datagram::Address("127.0.0.1",7001));
    
	int file = p2tp::Open("big_test_file");
    FileTransfer* fileobj = FileTransfer::file(file);
    
	int copy = p2tp::Open("big_test_file_copy",fileobj->root_hash());
  
	p2tp::Loop(TINT_MSEC);
    
    ASSERT_EQ(sizek<<10,p2tp::Size(copy));

    int count = 0;
    while (p2tp::SeqComplete(copy)!=size && count++<(1<<14))
        p2tp::Loop(TINT_MSEC);
    ASSERT_EQ(size,p2tp::SeqComplete(copy));
    
	p2tp::Close(file);
	p2tp::Close(copy);

	p2tp::Shutdown(sock1);
	//p2tp::Release(sock2);

}


int main (int argc, char** argv) {
	
	//bin::init();
	//bins::init();
	google::InitGoogleLogging(argv[0]);
	testing::InitGoogleTest(&argc, argv);
	int ret = RUN_ALL_TESTS();
	return ret;
	
}
