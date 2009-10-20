/*
 *  binstest.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/22/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include <time.h>
#include <set>
#include <gtest/gtest.h>
#include "bin.h"
#include "sbit.h"

TEST(BinsTest,AddSub) {
	bins b;
	b|=15;
	b-=1;
	ASSERT_TRUE(b.contains(2));
	ASSERT_TRUE(b.contains(14));
	ASSERT_FALSE(b.contains(3));
	ASSERT_FALSE(b.contains(22));
	ASSERT_TRUE(b.contains(12));
	b-=13;
	ASSERT_FALSE(b.contains(12));
	ASSERT_FALSE(b.contains(14));
	ASSERT_FALSE(b.contains(11));
	ASSERT_TRUE(b.contains(10));
}


TEST(BinsTest,Peaks) {
	bin::vec peaks = bin::peaks(11);
	ASSERT_EQ(3,peaks.size());
	ASSERT_EQ(15,peaks[0]);
	ASSERT_EQ(18,peaks[1]);
	ASSERT_EQ(19,peaks[2]);
}

TEST(BinsTest,Performance) {
	bins b;
	std::set<int> s;
	clock_t start, end;
	double b_time, s_time;
	int b_size, s_size;
	
	start = clock();
	for(int i=1; i<(1<<20); i++)
		b |= bin(i);
	//b_size = b.bits.size();
	end = clock();	
	b_time = ((double) (end - start)) / CLOCKS_PER_SEC;
	//ASSERT_EQ(1,b.bits.size());
	
	start = clock();
	for(int i=1; i<(1<<20); i++)
		s.insert(i);
	s_size = s.size();
	end = clock();
	s_time = ((double) (end - start)) / CLOCKS_PER_SEC;
	
	printf("bins: %f (%i), set: %f (%i)\n",b_time,b_size,s_time,s_size);
}

int main (int argc, char** argv) {
	bin::init();
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
