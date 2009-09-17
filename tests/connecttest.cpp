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
	char* b = (char*)malloc(size);
	for(int i=0; i<size; i++) 
		b[i] = (i%1024!=1023) ? ('A' + rand()%('Z'-'A')) : ('\n');
	write(f,b,size);
	free(b);
	close(f);
	
	struct sockaddr_in addr1, addr2;
	addr1.sin_family = AF_INET;
	addr1.sin_port = htons(7003);
	addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  
	addr2.sin_family = AF_INET;
	addr2.sin_port = htons(7004);
	addr2.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	  
    int sock1 = p2tp::Init(7003);
	int sock2 = p2tp::Init(7004);
	ASSERT_TRUE(sock1>=0);
	ASSERT_TRUE(sock2>=0);
	
	int file = p2tp::Open("big_test_file");
	p2tp::File& fileobj = * p2tp::File::file(file);
  
	int copy = p2tp::Open(fileobj.root_hash(),"big_test_file_copy");
	p2tp::File& copyobj = * p2tp::File::file(copy);
  
	int chan = p2tp::Connect(copy,sock1,addr2);
  
	p2tp::Loop();
	p2tp::Loop();
	p2tp::Channel& sendch = * p2tp::Channel::channel(chan+1);
	while (copyobj.status()!=p2tp::File::DONE) {
		p2tp::Loop();
		LOG(INFO)<<sendch.congestion_control().cwnd()<<" cwnd";
		//EXPECT_GE(1,sendch.congestion_control().cwnd());
	}
	
	ASSERT_EQ(p2tp::file_size(file),p2tp::file_size(copy));
	p2tp::Close(file);
	p2tp::Close(copy);

	p2tp::Shutdown(sock1);
	p2tp::Shutdown(sock2);

}


int main (int argc, char** argv) {
	
	bin::init();
	bins::init();
	google::InitGoogleLogging(argv[0]);
	testing::InitGoogleTest(&argc, argv);
	int ret = RUN_ALL_TESTS();
	return ret;
	
}
