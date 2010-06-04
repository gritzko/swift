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
    uint16_t joined  = binmap_t::join32to16(cell);
    EXPECT_EQ(correct,joined);
    
    uint32_t split = binmap_t::split16to32(correct);
    EXPECT_EQ(cell,split);
    
    EXPECT_EQ(binmap_t::NOJOIN,binmap_t::join32to16(cell|4));

}

TEST(BinsTest,SetGet) {

    binmap_t bs;
    bin64_t b3(1,0), b2(0,1), b4(0,2), b6(1,1), b7(2,0);
    bs.set(b3);
    //bs.dump("set done");
    EXPECT_EQ(binmap_t::FILLED,bs.get(b3));
    //bs.dump("set checkd");
    EXPECT_EQ(binmap_t::FILLED,bs.get(b2));
    //bs.dump("get b2 done");
    EXPECT_EQ(binmap_t::FILLED,bs.get(b3));
    //bs.dump("get b3 done");
    EXPECT_EQ(binmap_t::EMPTY,bs.get(b4));
    EXPECT_EQ(binmap_t::EMPTY,bs.get(b6));
    EXPECT_NE(binmap_t::FILLED,bs.get(b7));
    EXPECT_NE(binmap_t::EMPTY,bs.get(b7));
    EXPECT_EQ(binmap_t::FILLED,bs.get(b3));
    bs.set(bin64_t(1,1));
    EXPECT_EQ(binmap_t::FILLED,bs.get(bin64_t(2,0)));

}

TEST(BinsTest,Iterator) {
    binmap_t b;
    b.set(bin64_t(3,1));
    iterator i(&b,bin64_t(0,0),false);
    while (!i.solid())
        i.left();
    EXPECT_EQ(bin64_t(3,0),i.bin());
    EXPECT_EQ(false,i.deep());
    EXPECT_EQ(true,i.solid());
    EXPECT_EQ(binmap_t::EMPTY,*i);
    i.next();
    EXPECT_EQ(bin64_t(3,1),i.bin());
    EXPECT_EQ(false,i.deep());
    EXPECT_EQ(true,i.solid());
    EXPECT_EQ(binmap_t::FILLED,*i);
    i.next();
    EXPECT_TRUE(i.end());
}

TEST(BinsTest,Chess) {
    binmap_t chess16;
    for(int i=0; i<15; i++)
        chess16.set(bin64_t(0,i), (i&1)?binmap_t::FILLED:binmap_t::EMPTY);
    chess16.set(bin64_t(0,15), binmap_t::FILLED);
    for(int i=0; i<16; i++)
        EXPECT_EQ((i&1)?binmap_t::FILLED:binmap_t::EMPTY, chess16.get(bin64_t(0,i)));
    EXPECT_NE(binmap_t::FILLED,chess16.get(bin64_t(4,0)));
    EXPECT_NE(binmap_t::EMPTY,chess16.get(bin64_t(4,0)));
    for(int i=0; i<16; i+=2)
        chess16.set(bin64_t(0,i), binmap_t::FILLED);
    EXPECT_EQ(binmap_t::FILLED,chess16.get(bin64_t(4,0)));
    EXPECT_EQ(binmap_t::FILLED,chess16.get(bin64_t(2,3)));
    
    chess16.set(bin64_t(4,1),binmap_t::FILLED);
    EXPECT_EQ(binmap_t::FILLED,chess16.get(bin64_t(5,0)));
}

TEST(BinsTest,Staircase) {
    
    const int TOPLAYR = 44;
    binmap_t staircase;
    for(int i=0;i<TOPLAYR;i++)
        staircase.set(bin64_t(i,1),binmap_t::FILLED);
    
    EXPECT_NE(binmap_t::FILLED,staircase.get(bin64_t(TOPLAYR,0)));
    EXPECT_NE(binmap_t::EMPTY,staircase.get(bin64_t(TOPLAYR,0)));

    staircase.set(bin64_t(0,0),binmap_t::FILLED);
    EXPECT_EQ(binmap_t::FILLED,staircase.get(bin64_t(TOPLAYR,0)));

}

TEST(BinsTest,Hole) {
    
    binmap_t hole;
    hole.set(bin64_t(8,0),binmap_t::FILLED);
    hole.set(bin64_t(6,1),binmap_t::EMPTY);
    hole.set(bin64_t(6,2),binmap_t::EMPTY);
    EXPECT_EQ(binmap_t::FILLED,hole.get(bin64_t(6,0)));
    EXPECT_EQ(binmap_t::FILLED,hole.get(bin64_t(6,3)));
    EXPECT_NE(binmap_t::FILLED,hole.get(bin64_t(8,0)));
    EXPECT_NE(binmap_t::EMPTY,hole.get(bin64_t(8,0)));
    EXPECT_EQ(binmap_t::EMPTY,hole.get(bin64_t(6,1)));
    
}

TEST(BinsTest,Find){
    
    binmap_t hole;
    hole.set(bin64_t(4,0),binmap_t::FILLED);
    hole.set(bin64_t(1,1),binmap_t::EMPTY);
    hole.set(bin64_t(0,7),binmap_t::EMPTY);
    bin64_t f = hole.find(bin64_t(4,0)).left_foot();
    EXPECT_EQ(bin64_t(0,2),f);
    
}

TEST(BinsTest,Stripes) {
    
    binmap_t zebra;
    zebra.set(bin64_t(5,0));
    zebra.set(bin64_t(3,1),binmap_t::EMPTY);
    zebra.set(bin64_t(1,12),binmap_t::EMPTY);
    zebra.set(bin64_t(1,14),binmap_t::EMPTY);
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
    
    binmap_t zebra;
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

    binmap_t b;
    b.set(bin64_t(1,0));
    b.set(bin64_t(1,1));
    b.set(bin64_t(1,0),binmap_t::EMPTY);
    b.set(bin64_t(1,1),binmap_t::EMPTY);
    EXPECT_EQ(1,b.size());

}

TEST(BinsTest,Remove) {
    
    binmap_t b;
    b.set(bin64_t(5,0));
    binmap_t c;
    c.set(bin64_t(2,0));
    c.set(bin64_t(2,2));
    b.remove(c);
    EXPECT_EQ(binmap_t::EMPTY,b.get(bin64_t(2,0)));
    EXPECT_EQ(binmap_t::FILLED,b.get(bin64_t(2,1)));
    EXPECT_EQ(binmap_t::EMPTY,b.get(bin64_t(2,2)));
    EXPECT_EQ(binmap_t::FILLED,b.get(bin64_t(2,3)));
    EXPECT_EQ(binmap_t::FILLED,b.get(bin64_t(4,1)));
    
    binmap_t b16, b1024, b8192;
    b16.set(bin64_t(3,1));
    b1024.set(bin64_t(3,1));
    b1024.set(bin64_t(4,2));
    b1024.set(bin64_t(8,3));
    b8192.set(bin64_t(8,3));
    b8192.set(bin64_t(10,7));
    
    b1024.remove(b16);
    b1024.remove(b8192);
    
    EXPECT_EQ(binmap_t::EMPTY,b1024.get(bin64_t(3,1)));
    EXPECT_EQ(binmap_t::EMPTY,b1024.get(bin64_t(5,0)));
    EXPECT_EQ(binmap_t::EMPTY,b1024.get(bin64_t(9,1)));
    EXPECT_EQ(binmap_t::EMPTY,b1024.get(bin64_t(12,1)));
    EXPECT_EQ(binmap_t::FILLED,b1024.get(bin64_t(4,2)));
    
    b8192.set(bin64_t(2,3));
    b16.remove(b8192);
    EXPECT_EQ(binmap_t::EMPTY,b16.get(bin64_t(2,3)));
    EXPECT_EQ(binmap_t::FILLED,b16.get(bin64_t(2,2)));
    
}

TEST(BinsTest,FindFiltered) {
    
    binmap_t data, filter;
    data.set(bin64_t(2,0));
    data.set(bin64_t(2,2));
    data.set(bin64_t(1,7));
    filter.set(bin64_t(2,1));
    filter.set(bin64_t(1,4));
    filter.set(bin64_t(0,13));
    
    bin64_t x = data.find_filtered(filter,bin64_t(4,0)).left_foot();
    EXPECT_EQ(bin64_t(0,12),x);
    
}


TEST(BinsTest, Cover) {
    
    binmap_t b;
    b.set(bin64_t(2,0));
    b.set(bin64_t(4,1));
    EXPECT_EQ(bin64_t(4,1),b.cover(bin64_t(0,30)));
    EXPECT_EQ(bin64_t(2,0),b.cover(bin64_t(0,3)));
    EXPECT_EQ(bin64_t(2,0),b.cover(bin64_t(2,0)));
    //binmap_t c;
    //EXPECT_EQ(bin64_t::ALL,b.cover(bin64_t(0,30)));
    
}


TEST(BinsTest,FindFiltered2) {
    
    binmap_t data, filter;
    for(int i=0; i<1024; i+=2)
        data.set(bin64_t(0,i));
    for(int j=1; j<1024; j+=2)
        filter.set(bin64_t(0,j));
    filter.set(bin64_t(0,501),binmap_t::EMPTY);
    EXPECT_EQ(bin64_t(0,501),data.find_filtered(filter,bin64_t(10,0)).left_foot());
    data.set(bin64_t(0,501));
    EXPECT_EQ(bin64_t::NONE,data.find_filtered(filter,bin64_t(10,0)).left_foot());
    
}
    
TEST(BinsTest,CopyRange) {
    binmap_t data, add;
    data.set(bin64_t(2,0));
    data.set(bin64_t(2,2));
    data.set(bin64_t(1,7));
    add.set(bin64_t(2,1));
    add.set(bin64_t(1,4));
    add.set(bin64_t(0,13));
    add.set(bin64_t(5,118));
    data.range_copy(add, bin64_t(3,0));
    EXPECT_TRUE(binmap_t::is_mixed(data.get(bin64_t(3,0))));
    EXPECT_EQ(binmap_t::EMPTY,data.get(bin64_t(2,0)));
    EXPECT_EQ(binmap_t::FILLED,data.get(bin64_t(2,1)));
    EXPECT_EQ(binmap_t::EMPTY,data.get(bin64_t(1,6)));
    EXPECT_EQ(binmap_t::FILLED,data.get(bin64_t(1,7)));
}

TEST(BinsTest, Mass) {
    binmap_t b;
    b.set(bin64_t(6,0),binmap_t::FILLED);
    b.set(bin64_t(0,0),binmap_t::EMPTY);
    EXPECT_EQ(63,b.mass());
    EXPECT_FALSE(b.is_empty());
    b.clear();
    EXPECT_TRUE(b.is_empty());
    EXPECT_EQ(0,b.mass());

    binmap_t b50;
    for(int i=0; i<50; i++)
        b50.set(bin64_t(4,i*2));
    EXPECT_EQ(50<<4,b50.mass());
}

TEST(BinsTest,Twist) {
    binmap_t b;
    b.set(bin64_t(3,2));
    EXPECT_EQ(binmap_t::FILLED,b.get(bin64_t(3,2)));
    EXPECT_EQ(binmap_t::EMPTY,b.get(bin64_t(3,3)));
    b.twist(1<<3);
    EXPECT_EQ(binmap_t::FILLED,b.get(bin64_t(3,3)));
    EXPECT_EQ(binmap_t::EMPTY,b.get(bin64_t(3,2)));
    bin64_t tw = b.find(bin64_t(5,0),binmap_t::FILLED);
    while (tw.width()>(1<<3))
        tw = tw.left();
    tw = tw.twisted(1<<3);
    EXPECT_EQ(bin64_t(3,2),tw);
    b.twist(0);
    EXPECT_EQ(binmap_t::FILLED,b.get(bin64_t(3,2)));
    EXPECT_EQ(binmap_t::EMPTY,b.get(bin64_t(3,3)));
}

TEST(BinsTest,SeqLength) {
    binmap_t b;
    b.set(bin64_t(3,0));
    b.set(bin64_t(1,4));
    b.set(bin64_t(0,10));
    b.set(bin64_t(3,2));
    EXPECT_EQ(11,b.seq_length());
}

TEST(BinsTest,EmptyFilled) {
    // 1112 3312  2111 ....
    binmap_t b;
    
    EXPECT_TRUE(b.is_empty(bin64_t::ALL));
    
    b.set(bin64_t(1,0));
    b.set(bin64_t(0,2));
    b.set(bin64_t(0,6));
    b.set(bin64_t(1,5));
    b.set(bin64_t(0,9));
    
    EXPECT_FALSE(b.is_empty(bin64_t::ALL));
    
    EXPECT_TRUE(b.is_empty(bin64_t(2,3)));
    EXPECT_FALSE(b.is_filled(bin64_t(2,3)));
    EXPECT_TRUE(b.is_solid(bin64_t(2,3),binmap_t::MIXED));
    EXPECT_TRUE(b.is_filled(bin64_t(1,0)));
    EXPECT_TRUE(b.is_filled(bin64_t(1,5)));
    EXPECT_FALSE(b.is_filled(bin64_t(1,3)));
    
    b.set(bin64_t(0,3));
    b.set(bin64_t(0,7));
    b.set(bin64_t(0,8));
    
    EXPECT_TRUE(b.is_filled(bin64_t(2,0)));
    EXPECT_TRUE(b.is_filled(bin64_t(2,2)));    
    EXPECT_FALSE(b.is_filled(bin64_t(2,1)));    

    b.set(bin64_t(1,2));
    EXPECT_TRUE(b.is_filled(bin64_t(2,1)));    
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

TEST(BinsTest,RangeOpTest) {
    binmap_t a, b;
    a.set(bin64_t(0,0));
    a.set(bin64_t(0,2));
    b.set(bin64_t(0,1));
    b.set(bin64_t(0,3));
    a.range_or(b,bin64_t(1,0));
    EXPECT_TRUE(a.is_filled(bin64_t(1,0)));
    EXPECT_FALSE(a.is_filled(bin64_t(1,1)));
    
    binmap_t f, s;
    f.set(bin64_t(3,0));
    s.set(bin64_t(0,1));
    s.set(bin64_t(0,4));
    f.range_remove(s,bin64_t(2,1));
    
    EXPECT_TRUE(f.is_filled(bin64_t(2,0)));
    EXPECT_FALSE(f.is_filled(bin64_t(0,4)));
    EXPECT_TRUE(f.is_filled(bin64_t(0,5)));
    
    binmap_t z, x;
    z.set(bin64_t(1,0));
    z.set(bin64_t(1,2));
    x.set(bin64_t(0,1));
    x.set(bin64_t(0,1));
}

TEST(BinsTest,CoarseBitmap) {
    binmap_t b;
    b.set(bin64_t(4,0));
    union {uint16_t i16[2]; uint32_t i32;};
    b.to_coarse_bitmap(i16,bin64_t(5,0),0);
    EXPECT_EQ((1<<16)-1,i32);
    
    b.set(bin64_t(14,0));
    i32=0;
    b.to_coarse_bitmap(i16,bin64_t(15,0),10);
    EXPECT_EQ((1<<16)-1,i32);
    
    binmap_t rough;
    rough.set(bin64_t(1,0));
    rough.set(bin64_t(0,2));
    i32=0;
    rough.to_coarse_bitmap(i16,bin64_t(6,0),1);
    EXPECT_EQ(1,i32);
    
    binmap_t ladder;
    ladder.set(bin64_t(6,2));
    ladder.set(bin64_t(5,2));
    ladder.set(bin64_t(4,2));
    ladder.set(bin64_t(3,2));
    ladder.set(bin64_t(2,2));
    ladder.set(bin64_t(1,2));
    ladder.set(bin64_t(0,2));
    i32=0;
    ladder.to_coarse_bitmap(i16,bin64_t(8,0),3);
    EXPECT_EQ(0x00ff0f34,i32);
    
    binmap_t bin;
    bin.set(bin64_t(3,0));
    bin.set(bin64_t(0,8));
    i32 = 0;
    bin.to_coarse_bitmap(i16,bin64_t(4,0),0);
    EXPECT_EQ((1<<9)-1,i32);
    
    i32 = 0;
    bin.to_coarse_bitmap(i16,bin64_t(7,0),3);
    EXPECT_EQ(1,i32);
    
    i32 = 0;
    bin.to_coarse_bitmap(i16,bin64_t(4,0),3);
    EXPECT_EQ(1,i32);
    
    i32 = 0;
    bin.to_coarse_bitmap(i16,bin64_t(2,0),1);
    EXPECT_EQ(3,i32&3);

    uint64_t bigint;
    bigint = 0;
    binmap_t bm;
    bm.set(bin64_t(6,0));
    bm.to_coarse_bitmap((uint16_t*)&bigint,bin64_t(6,0),0);
    EXPECT_EQ( 0xffffffffffffffffULL, bigint );
    
}

/*TEST(BinsTest,AddSub) {
	binmap_t b;
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
	binmap_t b;
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
