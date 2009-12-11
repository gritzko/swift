/*
 *  bintest.cpp
 *  bin++
 *
 *  Created by Victor Grishchenko on 3/9/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "bin64.h"
#include <gtest/gtest.h>

TEST(Bin64Test,InitGet) {

    EXPECT_EQ(0x1,bin64_t(1,0));
    EXPECT_EQ(0xB,bin64_t(2,1));
    EXPECT_EQ(0x2,bin64_t(2,1).layer());
    EXPECT_EQ(34,bin64_t(34,2345).layer());
    EXPECT_EQ(0x7ffffffffULL,bin64_t(34,2345).tail_bits());
    EXPECT_EQ(1,bin64_t(2,1).offset());
    EXPECT_EQ(2345,bin64_t(34,2345).offset());
    EXPECT_EQ(1,bin64_t(0,123).tail_bit());
    EXPECT_EQ(1<<16,bin64_t(16,123).tail_bit());

}

TEST(Bin64Test,Navigation) {

    bin64_t mid(4,18);
    EXPECT_EQ(bin64_t(5,9),mid.parent());
    EXPECT_EQ(bin64_t(3,36),mid.left());
    EXPECT_EQ(bin64_t(3,37),mid.right());
    EXPECT_EQ(bin64_t(5,9),bin64_t(4,19).parent());
    bin64_t up32(30,1);
    EXPECT_EQ(bin64_t(31,0),up32.parent());

}

TEST(Bin64Test,Overflows) {

    EXPECT_FALSE(bin64_t(0,1).within(bin64_t::NONE));
    EXPECT_TRUE(bin64_t(0,1).within(bin64_t::ALL));
    EXPECT_EQ(0,bin64_t::none().width());
    EXPECT_EQ(bin64_t::none(),bin64_t::none().twisted(123));
    /*EXPECT_EQ(bin64_t::NONE.parent(),bin64_t::NONE);
    EXPECT_EQ(bin64_t::NONE.left(),bin64_t::NONE);
    EXPECT_EQ(bin64_t::NONE.right(),bin64_t::NONE);
    EXPECT_EQ(bin64_t::NONE,bin64_t(0,2345).left());
    EXPECT_EQ(bin64_t::NONE,bin64_t::ALL.parent());
*/
}

TEST(Bin64Test, Advanced) {

    EXPECT_EQ(4,bin64_t(2,3).width());
    EXPECT_FALSE(bin64_t(1,1234).is_base());
    EXPECT_TRUE(bin64_t(0,12345).is_base());
    EXPECT_EQ(bin64_t(0,2),bin64_t(1,1).left_foot());
    bin64_t peaks[64];
    int peak_count = bin64_t::peaks(7,peaks);
    EXPECT_EQ(3,peak_count);
    EXPECT_EQ(bin64_t(2,0),peaks[0]);
    EXPECT_EQ(bin64_t(1,2),peaks[1]);
    EXPECT_EQ(bin64_t(0,6),peaks[2]);

}

TEST(Bin64Test, Iteration) {
    bin64_t i(1,0);
    i = i.next_dfsio(1);
    EXPECT_EQ(bin64_t(1,1),i);
    i = i.next_dfsio(1);
    EXPECT_EQ(bin64_t(2,0),i);
    i = i.next_dfsio(1);
    EXPECT_EQ(bin64_t(1,2),i);
    i = i.next_dfsio(1);
    EXPECT_EQ(bin64_t(1,3),i);
    i = i.next_dfsio(1);
    EXPECT_EQ(bin64_t(2,1),i);
    i = i.next_dfsio(1);
    EXPECT_EQ(bin64_t(3,0),i);
}

TEST(Bin64Test, Bits) {
    bin64_t all = bin64_t::ALL, none = bin64_t::NONE, big = bin64_t(40,18);
    uint32_t a32 = all.to32(), n32 = none.to32(), b32 = big.to32();
    EXPECT_EQ(0x7fffffff,a32);
    EXPECT_EQ(0xffffffff,n32);
    EXPECT_EQ(bin64_t::NONE32,b32);
}

int main (int argc, char** argv) {

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();

}
