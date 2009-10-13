/*
 *  transfertest.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/7/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include <gtest/gtest.h>
#include "p2tp.h"

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
    FileTransfer* seed = new FileTransfer(Sha1Hash::ZERO,BTF);
    EXPECT_TRUE(A==seed->hashes[0]);
    EXPECT_TRUE(E==seed->hashes[bin64_t(0,4)]);
    EXPECT_TRUE(ABCD==seed->hashes[bin64_t(2,0)]);
    EXPECT_TRUE(ROOT==seed->root_hash);
    EXPECT_TRUE(ABCD==seed->peak_hashes[0]);
    EXPECT_TRUE(E==seed->peak_hashes[1]);
    EXPECT_TRUE(ROOT==seed->root_hash);
    EXPECT_EQ(4100,seed->size);
    EXPECT_EQ(5,seed->sizek);
    EXPECT_EQ(4100,seed->complete);
    EXPECT_EQ(4100,seed->seq_complete);
    
    // retrieve it
    unlink("copy");
    FileTransfer::instance = 1;
    FileTransfer* leech = new FileTransfer(seed->root_hash,"copy");
    // transfer peak hashes
    for(int i=0; i<seed->peak_count; i++)
        leech->OfferHash(seed->peaks[i],seed->peak_hashes[i]);
    ASSERT_EQ(5<<10,leech->size);
    ASSERT_EQ(5,leech->sizek);
    ASSERT_EQ(0,leech->complete);
    // transfer data and hashes
    //           ABCD            E000
    //     AB         CD       E0    0
    //  AAAA BBBB  CCCC DDDD  E  0  0  0
    leech->OfferHash(bin64_t(1,0), seed->hashes[bin64_t(1,0)]);
    leech->OfferHash(bin64_t(1,1), seed->hashes[bin64_t(1,1)]);
    for (int i=0; i<5; i++) {
        if (i==2) {
            delete leech;
            FileTransfer::instance = 1;
            leech = new FileTransfer(seed->root_hash,"copy");
            EXPECT_EQ(2,leech->completek);
            //leech->OfferHash(bin64_t(1,0), seed->hashes[bin64_t(1,0)]);
            //leech->OfferHash(bin64_t(1,1), seed->hashes[bin64_t(1,1)]);
        }
        bin64_t next = leech->picker->Pick(seed->ack_out,0);
        ASSERT_NE(bin64_t::NONE,next);
        uint8_t buf[1024];         //size_t len = seed->storer->ReadData(next,&buf);
        size_t len = pread(seed->fd,buf,1024,next.base_offset()<<10); // FIXME TEST FOR ERROR
        bin64_t sibling = next.sibling();
        leech->OfferHash(sibling, seed->hashes[sibling]); // i=4 => out of bounds
        EXPECT_TRUE(leech->OfferData(next, buf, len));
    }
    EXPECT_EQ(4100,leech->size);
    EXPECT_EQ(5,leech->sizek);
    EXPECT_EQ(4100,leech->complete);
    EXPECT_EQ(4100,leech->seq_complete);
    
}
/*
 FIXME
 - always rehashes (even fresh files)
 - different heights => bins::remove is buggy
 */

int main (int argc, char** argv) {
    
    unlink("/tmp/.70196e6065a42835b1f08227ac3e2fb419cf78c8.0.hashes");
    unlink("/tmp/.70196e6065a42835b1f08227ac3e2fb419cf78c8.0.peaks");
    unlink("/tmp/.70196e6065a42835b1f08227ac3e2fb419cf78c8.1.hashes");
    unlink("/tmp/.70196e6065a42835b1f08227ac3e2fb419cf78c8.1.peaks");
    unlink("/tmp/.70196e6065a42835b1f08227ac3e2fb419cf78c8.2.hashes");
    unlink("/tmp/.70196e6065a42835b1f08227ac3e2fb419cf78c8.2.peaks");
    
    unlink(BTF);
    unlink("copy");
        
	int f = open(BTF,O_RDWR|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
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
