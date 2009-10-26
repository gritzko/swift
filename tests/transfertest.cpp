/*
 *  transfertest.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/7/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include <gtest/gtest.h>
#include <glog/logging.h>
#include "p2tp.h"
#include "compat/util.h"
#ifdef _MSC_VER
#include "compat/unixio.h"
#endif

using namespace p2tp;

const char* BTF = "big_test_file";

Sha1Hash A,B,C,D,E,AB,CD,ABCD,E0,E000,ABCDE000,ROOT;


TEST(TransferTest,TransferFile) {

    AB = Sha1Hash(A,B);
    CD = Sha1Hash(C,D);
    ABCD = Sha1Hash(AB,CD);
    E0 = Sha1Hash(E,Sha1Hash::ZERO);
    E000 = Sha1Hash(E0,Sha1Hash::ZERO);
    ABCDE000 = Sha1Hash(ABCD,E000);
    ROOT = ABCDE000;
    for (bin64_t pos(3,0); pos!=bin64_t::ALL; pos=pos.parent()) {
        ROOT = Sha1Hash(ROOT,Sha1Hash::ZERO);
        //printf("m %lli %s\n",(uint64_t)pos.parent(),ROOT.hex().c_str());
    }

    // submit a new file
    FileTransfer* seed = new FileTransfer(BTF);
    EXPECT_TRUE(A==seed->hash(0));
    EXPECT_TRUE(E==seed->hash(bin64_t(0,4)));
    EXPECT_TRUE(ABCD==seed->hash(bin64_t(2,0)));
    EXPECT_TRUE(ROOT==seed->root_hash());
    EXPECT_TRUE(ABCD==seed->peak_hash(0));
    EXPECT_TRUE(E==seed->peak_hash(1));
    EXPECT_TRUE(ROOT==seed->root_hash());
    EXPECT_EQ(4100,seed->size());
    EXPECT_EQ(5,seed->size_kilo());
    EXPECT_EQ(4100,seed->complete());
    EXPECT_EQ(4100,seed->seq_complete());

    // retrieve it
    unlink("copy");
    FileTransfer::instance = 1;
    FileTransfer* leech = new FileTransfer("copy",seed->root_hash());
    // transfer peak hashes
    for(int i=0; i<seed->peak_count(); i++)
        leech->OfferHash(seed->peak(i),seed->peak_hash(i));
    ASSERT_EQ(5<<10,leech->size());
    ASSERT_EQ(5,leech->size_kilo());
    ASSERT_EQ(0,leech->complete());
    // transfer data and hashes
    //           ABCD            E000
    //     AB         CD       E0    0
    //  AAAA BBBB  CCCC DDDD  E  0  0  0
    // calculated leech->OfferHash(bin64_t(1,0), seed->hashes[bin64_t(1,0)]);
    leech->OfferHash(bin64_t(1,1), seed->hash(bin64_t(1,1)));
    for (int i=0; i<5; i++) {
        if (i==2) { // now: stop, save, start
            delete leech;
            FileTransfer::instance = 1;
            leech = new FileTransfer("copy",seed->root_hash());
            EXPECT_EQ(2,leech->complete_kilo());
        }
        bin64_t next = leech->picker().Pick(seed->ack_out(),0);
        ASSERT_NE(bin64_t::NONE,next);
        ASSERT_TRUE(next.base_offset()<5);
        uint8_t buf[1024];         //size_t len = seed->storer->ReadData(next,&buf);
        size_t len = pread(seed->file_descriptor(),buf,1024,next.base_offset()<<10);
        bin64_t sibling = next.sibling();
        if (sibling.base_offset()<seed->size_kilo())
            leech->OfferHash(sibling, seed->hash(sibling));
        uint8_t memo = *buf;
        *buf = 'z';
        EXPECT_FALSE(leech->OfferData(next, buf, len));
        *buf = memo;
        EXPECT_TRUE(leech->OfferData(next, buf, len));
    }
    EXPECT_EQ(4100,leech->size());
    EXPECT_EQ(5,leech->size_kilo());
    EXPECT_EQ(4100,leech->complete());
    EXPECT_EQ(4100,leech->seq_complete());

}
/*
 FIXME
 - always rehashes (even fresh files)
 */

int main (int argc, char** argv) {

	google::InitGoogleLogging(argv[0]);

	std::string tempdir = gettmpdir();
    unlink((tempdir + std::string(".70196e6065a42835b1f08227ac3e2fb419cf78c8.0.hashes")).c_str());
    unlink((tempdir + std::string(".70196e6065a42835b1f08227ac3e2fb419cf78c8.0.peaks")).c_str());
    unlink((tempdir + std::string(".70196e6065a42835b1f08227ac3e2fb419cf78c8.1.hashes")).c_str());
    unlink((tempdir + std::string(".70196e6065a42835b1f08227ac3e2fb419cf78c8.1.peaks")).c_str());
    unlink((tempdir + std::string(".70196e6065a42835b1f08227ac3e2fb419cf78c8.2.hashes")).c_str());
    unlink((tempdir + std::string(".70196e6065a42835b1f08227ac3e2fb419cf78c8.2.peaks")).c_str());

    unlink(BTF);
    unlink("copy");

	int f = open(BTF,O_RDWR|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (f < 0)
	{
		PLOG(FATAL)<< "Error opening " << BTF << "\n";
		return -1;
	}
    uint8_t buf[1024];
    memset(buf,'A',1024);
    A = Sha1Hash(buf,1024);
    write(f,buf,1024);
    memset(buf,'B',1024);
    B = Sha1Hash(buf,1024);
    write(f,buf,1024);
    memset(buf,'C',1024);
    C = Sha1Hash(buf,1024);
    write(f,buf,1024);
    memset(buf,'D',1024);
    D = Sha1Hash(buf,1024);
    write(f,buf,1024);
    memset(buf,'E',4);
    E = Sha1Hash(buf,4);
    write(f,buf,4);
	close(f);

	testing::InitGoogleTest(&argc, argv);
	int ret = RUN_ALL_TESTS();

    return ret;
}
