/*
 *  bintest.cpp
 *  bin++
 *
 *  Created by Victor Grishchenko on 3/9/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include "bin64.h"
#include <gtest/gtest.h>

TEST(BinTest,InitGet) {

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

TEST(BinTest,Navigation) {

    bin64_t mid(4,18);
    EXPECT_EQ(bin64_t(5,9),mid.parent());
    EXPECT_EQ(bin64_t(3,36),mid.left());
    EXPECT_EQ(bin64_t(3,37),mid.right());
    EXPECT_EQ(bin64_t(5,9),bin64_t(4,19).parent());
    bin64_t up32(30,1);
    EXPECT_EQ(bin64_t(31,0),up32.parent());

}

TEST(BinTest,Overflows) {
    
    /*EXPECT_EQ(bin64_t::NONE.parent(),bin64_t::NONE);
    EXPECT_EQ(bin64_t::NONE.left(),bin64_t::NONE);
    EXPECT_EQ(bin64_t::NONE.right(),bin64_t::NONE);
    EXPECT_EQ(bin64_t::NONE,bin64_t(0,2345).left());
    EXPECT_EQ(bin64_t::NONE,bin64_t::ALL.parent());
*/
}

int main (int argc, char** argv) {
	
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
	
}
