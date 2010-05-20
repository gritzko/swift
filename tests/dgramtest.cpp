/*
 *  dgramtest.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/13/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include <gtest/gtest.h>
//#include <glog/logging.h>
#include "datagram.h"
#include "swift.h" // Arno: for LibraryInit

using namespace swift;

TEST(Datagram, AddressTest) {
    Address addr("127.0.0.1:1000");
    EXPECT_EQ(INADDR_LOOPBACK,addr.ipv4());
    EXPECT_EQ(1000,addr.port());
    Address das2("node300.das2.ewi.tudelft.nl:20000");
    Address das2b("130.161.211.200:20000");
    EXPECT_EQ(das2.ipv4(),das2b.ipv4());
    EXPECT_EQ(20000,das2.port());
}


TEST(Datagram, BinaryTest) {
	SOCKET socket = Datagram::Bind(7001);
	ASSERT_TRUE(socket>0);
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(7001);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	Datagram d(socket,addr); //Address(7001));
	const char * text = "text";
	const uint8_t num8 = 0xab;
	const uint16_t num16 = 0xabcd;
	const uint32_t num32 = 0xabcdef01;
	const uint64_t num64 = 0xabcdefabcdeffULL;
	d.PushString(text);
	d.Push8(num8);
	d.Push16(num16);
	d.Push32(num32);
	d.Push64(num64);
	char buf[1024];
	int i;
	for(i=0; i<d.size(); i++)
		sprintf(buf+i*2,"%02x",*(unsigned char*)(*d+i));
	buf[i*2] = 0;
	EXPECT_STREQ("74657874ababcdabcdef01000abcdefabcdeff",buf);
	int datalen = strlen(text)+1+2+4+8;
	ASSERT_EQ(datalen,d.Send());
    SOCKET socks[1] = {socket};
    // Arno: timeout 0 gives undeterministic behaviour on win32
	SOCKET waitsocket = Datagram::Wait(1000000);
	ASSERT_EQ(socket,waitsocket);
	Datagram rcv(waitsocket);
	ASSERT_EQ(datalen,rcv.Recv());
	char* rbuf;
	int pl = rcv.Pull((uint8_t**)&rbuf,strlen(text));
	memcpy(buf,rbuf,pl);
	buf[pl]=0;
	uint8_t rnum8 = rcv.Pull8();
	uint16_t rnum16 = rcv.Pull16();
	uint32_t rnum32 = rcv.Pull32();
	uint64_t rnum64 = rcv.Pull64();
	EXPECT_STREQ("text",buf);
	EXPECT_EQ(0xab,rnum8);
	EXPECT_EQ(0xabcd,rnum16);
	EXPECT_EQ(0xabcdef01,rnum32);
	EXPECT_EQ(0xabcdefabcdeffULL,rnum64);
	Datagram::Close(socket);
}

TEST(Datagram,TwoPortTest) {
	int sock1 = Datagram::Bind(10001);
	int sock2 = Datagram::Bind(10002);
	ASSERT_TRUE(sock1>0);
	ASSERT_TRUE(sock2>0);
	/*struct sockaddr_in addr1, addr2;
	addr1.sin_family = AF_INET;
	addr1.sin_port = htons(10001);
	addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr2.sin_family = AF_INET;
	addr2.sin_port = htons(10002);
	addr2.sin_addr.s_addr = htonl(INADDR_LOOPBACK);*/

	Datagram send(sock1,Address(10002));
	send.Push32(1234);
	send.Send();

    SOCKET socks[2] = {sock1,sock2};
    // Arno: timeout 0 gives undeterministic behaviour on win32
	EXPECT_EQ(sock2,Datagram::Wait(1000000));
	Datagram recv(sock2);
	recv.Recv();
	uint32_t test = recv.Pull32();
	ASSERT_EQ(1234,test);

	Datagram::Close(sock1);
	Datagram::Close(sock2);
}

int main (int argc, char** argv) {

	swift::LibraryInit();

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();

}
