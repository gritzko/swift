/*
 *  hashtest.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/12/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include <fcntl.h>
#include "bin64.h"
#include <gtest/gtest.h>
#include "hashtree.h"

using namespace swift;

char hash123[] = "a8fdc205a9f19cc1c7507a60c4f01b13d11d7fd0";
char rooth123[] = "d0bdb8ba28076d84d2b3a0e62521b998e42349a1";

TEST(Sha1HashTest,Trivial) {
	Sha1Hash hash("123\n");
	EXPECT_STREQ(hash123,hash.hex().c_str());
}


TEST(Sha1HashTest,OfferDataTest) {
	Sha1Hash roothash123(true,hash123);
	for(bin64_t pos(0,0); pos!=bin64_t::ALL; pos=pos.parent())
		roothash123 = Sha1Hash(roothash123,Sha1Hash::ZERO);
    unlink("123");
    EXPECT_STREQ(rooth123,roothash123.hex().c_str());
	HashTree tree("123",roothash123);
    tree.OfferHash(bin64_t(0,0),Sha1Hash(true,hash123));
	ASSERT_EQ(1,tree.packet_size());
    ASSERT_TRUE(tree.OfferData(bin64_t(0,0), "123\n", 4));
    unlink("123");
	ASSERT_EQ(4,tree.size());
}


TEST(Sha1HashTest,SubmitTest) {
    FILE* f123 = fopen("123","wb+");
    fprintf(f123, "123\n");
    fclose(f123);
    HashTree ht123("123");
    EXPECT_STREQ(hash123,ht123.hash(bin64_t(0,0)).hex().c_str());
    EXPECT_STREQ(rooth123,ht123.root_hash().hex().c_str());
    EXPECT_EQ(4,ht123.size());
}


/*TEST(Sha1HashTest,HashFileTest) {
	uint8_t a [1024], b[1024], c[1024];
	memset(a,'a',1024);
	memset(b,'b',1024);
	memset(c,'c',1024);
	Sha1Hash aaahash(a,1024), bbbhash(b,1024), ccchash(c,1024);
	Sha1Hash abhash(aaahash,bbbhash), c0hash(ccchash,Sha1Hash::ZERO);
	Sha1Hash aabbccroot(abhash,c0hash);
	for(bin pos=bin(7); pos<bin::ALL; pos=pos.parent())
		aabbccroot = Sha1Hash(aabbccroot,Sha1Hash::ZERO);
	int f = open("testfile",O_RDWR|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	write(f,a,1024);
	write(f,b,1024);
	write(f,c,1024);
	HashTree filetree(f);
	close(f);
	ASSERT_TRUE(aabbccroot==filetree.root);
	EXPECT_EQ(2,filetree.peaks.size());
	EXPECT_TRUE(aaahash==filetree[1]);
	HashTree bootstree(filetree.root);
	EXPECT_EQ( HashTree::DUNNO, bootstree.offer(filetree.peaks[0].first,filetree.peaks[0].second) );
	EXPECT_EQ( HashTree::PEAK_ACCEPT, bootstree.offer(filetree.peaks[1].first,filetree.peaks[1].second) );
	EXPECT_EQ( 3, bootstree.length );
	EXPECT_EQ( 4, bootstree.mass );
	EXPECT_EQ( HashTree::DUNNO, bootstree.offer(1,aaahash) );
	EXPECT_EQ( HashTree::ACCEPT, bootstree.offer(2,bbbhash) );
	EXPECT_TRUE ( bootstree.bits[3]==abhash );
	EXPECT_TRUE ( bootstree.bits[1]==aaahash );
	EXPECT_TRUE ( bootstree.bits[2]==bbbhash );
	EXPECT_FALSE ( bootstree.bits[2]==aaahash );
}*/


int main (int argc, char** argv) {
	//bin::init();

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();

}
