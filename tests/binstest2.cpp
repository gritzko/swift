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
#include "bins.h"


TEST(BinsTest,Routines) {

    uint32_t cell = (3<<10) | (3<<14) | (3<<0);
    uint16_t correct = (1<<5) | (1<<7) | (1<<0);
    uint16_t joined  = bins::join32to16(cell);
    EXPECT_EQ(correct,joined);
    
    uint32_t split = bins::split16to32(correct);
    EXPECT_EQ(cell,split);
    
    EXPECT_EQ(bins::NOJOIN,bins::join32to16(cell|4));

}

TEST(BinsTest,SetGet) {

    bins bs;
    bin64_t b3(1,0), b2(0,1), b4(0,2), b6(1,1), b7(2,0);
    bs.set(b3);
    //bs.dump("set done");
    EXPECT_EQ(bins::FILLED,bs.get(b3));
    //bs.dump("set checkd");
    EXPECT_EQ(bins::FILLED,bs.get(b2));
    //bs.dump("get b2 done");
    EXPECT_EQ(bins::FILLED,bs.get(b3));
    //bs.dump("get b3 done");
    EXPECT_EQ(bins::EMPTY,bs.get(b4));
    EXPECT_EQ(bins::EMPTY,bs.get(b6));
    EXPECT_NE(bins::FILLED,bs.get(b7));
    EXPECT_NE(bins::EMPTY,bs.get(b7));
    EXPECT_EQ(bins::FILLED,bs.get(b3));
    bs.set(bin64_t(1,1));
    EXPECT_EQ(bins::FILLED,bs.get(bin64_t(2,0)));

}

TEST(BinsTest,Iterator) {
    bins b;
    b.set(bin64_t(3,1));
    iterator i(&b,0,false);
    while (!i.solid())
        i.left();
    EXPECT_EQ(bin64_t(3,0),i.bin());
    EXPECT_EQ(false,i.deep());
    EXPECT_EQ(true,i.solid());
    EXPECT_EQ(bins::EMPTY,*i);
    i.next();
    EXPECT_EQ(bin64_t(3,1),i.bin());
    EXPECT_EQ(false,i.deep());
    EXPECT_EQ(true,i.solid());
    EXPECT_EQ(bins::FILLED,*i);
    i.next();
    EXPECT_TRUE(i.end());
}

TEST(BinsTest,Chess) {
    bins chess16;
    for(int i=0; i<15; i++)
        chess16.set(bin64_t(0,i), (i&1)?bins::FILLED:bins::EMPTY);
    chess16.set(bin64_t(0,15), bins::FILLED);
    for(int i=0; i<16; i++)
        EXPECT_EQ((i&1)?bins::FILLED:bins::EMPTY, chess16.get(bin64_t(0,i)));
    EXPECT_NE(bins::FILLED,chess16.get(bin64_t(4,0)));
    EXPECT_NE(bins::EMPTY,chess16.get(bin64_t(4,0)));
    for(int i=0; i<16; i+=2)
        chess16.set(bin64_t(0,i), bins::FILLED);
    EXPECT_EQ(bins::FILLED,chess16.get(bin64_t(4,0)));
    EXPECT_EQ(bins::FILLED,chess16.get(bin64_t(2,3)));
    
    chess16.set(bin64_t(4,1),bins::FILLED);
    EXPECT_EQ(bins::FILLED,chess16.get(bin64_t(5,0)));
}

TEST(BinsTest,Staircase) {
    
    const int TOPLAYR = 44;
    bins staircase;
    for(int i=0;i<TOPLAYR;i++)
        staircase.set(bin64_t(i,1),bins::FILLED);
    
    EXPECT_NE(bins::FILLED,staircase.get(bin64_t(TOPLAYR,0)));
    EXPECT_NE(bins::EMPTY,staircase.get(bin64_t(TOPLAYR,0)));

    staircase.set(bin64_t(0,0),bins::FILLED);
    EXPECT_EQ(bins::FILLED,staircase.get(bin64_t(TOPLAYR,0)));

}

TEST(BinsTest,Hole) {
    
    bins hole;
    hole.set(bin64_t(8,0),bins::FILLED);
    hole.set(bin64_t(6,1),bins::EMPTY);
    hole.set(bin64_t(6,2),bins::EMPTY);
    EXPECT_EQ(bins::FILLED,hole.get(bin64_t(6,0)));
    EXPECT_EQ(bins::FILLED,hole.get(bin64_t(6,3)));
    EXPECT_NE(bins::FILLED,hole.get(bin64_t(8,0)));
    EXPECT_NE(bins::EMPTY,hole.get(bin64_t(8,0)));
    EXPECT_EQ(bins::EMPTY,hole.get(bin64_t(6,1)));
    
}

TEST(BinsTest,Find){
    
    bins hole;
    hole.set(bin64_t(4,0),bins::FILLED);
    hole.set(bin64_t(1,1),bins::EMPTY);
    hole.set(bin64_t(0,7),bins::EMPTY);
    bin64_t f = hole.find(bin64_t(4,0),0);
    EXPECT_EQ(bin64_t(0,2),f);
    
}

TEST(BinsTest,Stripes) {
    
    bins zebra;
    zebra.set(bin64_t(5,0));
    zebra.set(bin64_t(3,1),bins::EMPTY);
    zebra.set(bin64_t(1,12),bins::EMPTY);
    zebra.set(bin64_t(1,14),bins::EMPTY);
    int count;
    uint64_t *stripes = zebra.get_stripes(count);
    EXPECT_EQ(9,count);
    EXPECT_EQ(0,stripes[0]);
    EXPECT_EQ(0,stripes[1]);
    EXPECT_EQ(8,stripes[2]);
    EXPECT_EQ(16,stripes[3]);
    EXPECT_EQ(24,stripes[4]);
    EXPECT_EQ(26,stripes[5]);
    EXPECT_EQ(28,stripes[6]);
    EXPECT_EQ(30,stripes[7]);
    EXPECT_EQ(32,stripes[8]);
    free(stripes);

}

TEST(BinsTest,StripesAgg) {
    
    bins zebra;
    zebra.set(bin64_t(0,1));
    zebra.set(bin64_t(0,2));
    int count;
    uint64_t *stripes = zebra.get_stripes(count);
    EXPECT_EQ(3,count);
    EXPECT_EQ(0,stripes[0]);
    EXPECT_EQ(1,stripes[1]);
    EXPECT_EQ(3,stripes[2]);
    free(stripes);
    
}    

TEST(BinsTest,Alloc) {

    bins b;
    b.set(bin64_t(1,0));
    b.set(bin64_t(1,1));
    b.set(bin64_t(1,0),bins::EMPTY);
    b.set(bin64_t(1,1),bins::EMPTY);
    EXPECT_EQ(1,b.size());

}

TEST(BinsTest,Remove) {
    
    bins b;
    b.set(bin64_t(5,0));
    bins c;
    c.set(bin64_t(2,0));
    c.set(bin64_t(2,2));
    b.remove(c);
    EXPECT_EQ(bins::EMPTY,b.get(bin64_t(2,0)));
    EXPECT_EQ(bins::FILLED,b.get(bin64_t(2,1)));
    EXPECT_EQ(bins::EMPTY,b.get(bin64_t(2,2)));
    EXPECT_EQ(bins::FILLED,b.get(bin64_t(2,3)));
    EXPECT_EQ(bins::FILLED,b.get(bin64_t(4,1)));
    
    bins b16, b1024, b8192;
    b16.set(bin64_t(3,1));
    b1024.set(bin64_t(3,1));
    b1024.set(bin64_t(4,2));
    b1024.set(bin64_t(8,3));
    b8192.set(bin64_t(8,3));
    b8192.set(bin64_t(10,7));
    
    b1024.remove(b16);
    b1024.remove(b8192);
    
    EXPECT_EQ(bins::EMPTY,b1024.get(bin64_t(3,1)));
    EXPECT_EQ(bins::EMPTY,b1024.get(bin64_t(5,0)));
    EXPECT_EQ(bins::EMPTY,b1024.get(bin64_t(9,1)));
    EXPECT_EQ(bins::EMPTY,b1024.get(bin64_t(12,1)));
    EXPECT_EQ(bins::FILLED,b1024.get(bin64_t(4,2)));
    
    b8192.set(bin64_t(2,3));
    b16.remove(b8192);
    EXPECT_EQ(bins::EMPTY,b16.get(bin64_t(2,3)));
    EXPECT_EQ(bins::FILLED,b16.get(bin64_t(2,2)));
    
}

TEST(BinheapTest,Eat) {
    
    binheap b;
    b.push(bin64_t(0,1));
    b.push(bin64_t(0,3));
    b.push(bin64_t(2,0));
    b.push(bin64_t(2,4));
    
    EXPECT_EQ(bin64_t(2,0),b.pop());
    EXPECT_EQ(bin64_t(2,4),b.pop());
    EXPECT_EQ(bin64_t::none(),b.pop());
    
    for (int i=0; i<64; i++) {
        b.push(bin64_t(0,i));
    }
    b.push(bin64_t(5,0));
    EXPECT_EQ(bin64_t(5,0),b.pop());
    for (int i=32; i<64; i++)
        EXPECT_EQ(bin64_t(0,i),b.pop());
        
}

/*TEST(BinsTest,AddSub) {
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
}*/

int main (int argc, char** argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
