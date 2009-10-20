/*
 *  sbit2test.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 4/1/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */

#include "bin.h"
#include "sbit.h"
#include <gtest/gtest.h>

TEST(SbitTest,Init) {
	bins s;
	s.set(1);
	EXPECT_TRUE(s.get(1));
	EXPECT_FALSE(s.get(2));
	EXPECT_FALSE(s.get(3));
	s.set(3);
	EXPECT_TRUE(s.get(1));
	EXPECT_TRUE(s.get(2));
	EXPECT_TRUE(s.get(3));
}


TEST(SbitTest,Expand) {
	bins s;
	s.set(1);
	s.set(34);
	EXPECT_TRUE(s.get(1));
	EXPECT_FALSE(s.get(2));
	EXPECT_TRUE(s.get(32));
	EXPECT_TRUE(s.get(33));
	EXPECT_FALSE(s.get(35));
	s.set(31);
	s.set(62);
	EXPECT_TRUE(s.get(3));
	EXPECT_TRUE(s.get(63));
}


TEST(SbitTest,Chess) {
	bins s;
	s.set(7);
	s.set(22);
	EXPECT_FALSE(s.get(14));
	EXPECT_FALSE(s.get(30));
	EXPECT_FALSE(s.get(29));
	s.set(29);
	EXPECT_FALSE(s.get(31));
	s.set(10);
	EXPECT_FALSE(s.get(31));
	s.set(11);
	EXPECT_FALSE(s.get(31));
	s.set(12);
	EXPECT_TRUE(s.get(31));
}


TEST(SbitTest,Hole) {
	bins b;
	int h=14;
	for(int i=0; i<h; i++)
		b.set(bin(i,1));
	ASSERT_FALSE(b.get(bin(h,0)));
	for(int i=1; i<=bin(h,0); i++)
		ASSERT_EQ( !bin::all1(i), b.get(bin(i)) );
	b.set(1);
	ASSERT_TRUE(b.get(bin(h,0)));
}


TEST(SbitTest,Zebra) {
	bins b;
	int height=9, width=1<<height;
	bin peak = bin(height+3,0);
	bin base[1024];
	for(int i=0; i<width; i+=1)
		base[i] = bin(3,i);
	for(int j=0; j<3; j++) {
		for(int i=0; i<width; i+=1) {
			base[i] = base[i].left();
			b.set(base[i]);
			base[i] = base[i].sibling();
		}
	}
	ASSERT_FALSE(b.get(peak));
	for(int i=0; i<width; i+=1)
		b.set(base[i]);
	ASSERT_TRUE(b.get(peak));
}


TEST(SbitTest,Overlaps) {
	bins b;
	for(int i=0; i<400; i++)
		b.set(bin(0,1000+i*32));
	b.set(63);
	b.set(15);
	b.set(27);
	b.set(9);
	EXPECT_EQ(true,b.get(9));
	EXPECT_EQ(true,b.get(30));
	EXPECT_EQ(true,b.get(63));
	EXPECT_EQ(false,b.get(65));
}


TEST(SbitTest,Random) {
	for(int round=0; round<100; round++) {
		bin::vec v;
		bins b;
		for(int i=0; i<40; i++) {
			bin x = random()%8191+1;
			//printf("(%i,%i) ",x.layer(),x.offset());
			v.push_back(x);
			b.set(x);
		}
		//printf("\n> ");
		bin::order(&v);
		///for(int i=0; i<v.size(); i++)
		//	printf("(%i,%i) ",v[i].layer(),v[i].offset());
		//printf("\n");
		for(int i=0; i<v.size(); i++) {
			EXPECT_TRUE(b.get(v[i]))<<"where ("<<(int)v[i].layer()<<","<<v[i].offset()<<")";
			if (!b.get(v[i]))
				b.get(v[i]);
		}
		for(int n=1; n<8192; n+=rand()%10) {
			bool in = false;
			for(int i=0; i<v.size(); i++)
				if (v[i].contains(n))
					in = true;
			EXPECT_EQ(in,b.get(n));
		}
	}
}


TEST(SbitTest,Full) {
	bins b;
	for(int i=0; i<32; i++)
		b.set(bin(0,i));
	ASSERT_TRUE(b.get(63));
}


TEST(SbitTest,OrAnd) {
	bins a, b;
	a.set(15);
	b.set(30);
	b.set(62);
	a|=b;
	EXPECT_TRUE(a.get(63));
	a&=b;
	EXPECT_TRUE(a.get(30));
	EXPECT_TRUE(a.get(62));
	EXPECT_FALSE(a.get(15));
	EXPECT_FALSE(a.get(63));
}


TEST(SbitTest, Iterator) {
	bins b;
	b.set(7);
	b.set(14);
	b.set(18);
	b.set(21);
	bin::vec memo;
	for (bins::bin_iterator i = b.begin(); i!=b.end(); ++i)
		memo.push_back(*i);
	//for(int i=0; i<memo.size(); i++)
	//	printf("%i\n",(int)memo[i]);
	EXPECT_EQ(2,memo.size());
	EXPECT_EQ(15,memo[0]);
	EXPECT_EQ(22,memo[1]);
}


TEST(SbitTest,Diff) {
	bins a,b;
	a.set(127);
	b.set(1);
	a-=b;
	EXPECT_EQ(false,a.get(3));
	bins::bin_iterator i = a.begin();
	EXPECT_EQ(2,*i);
	++i;
	EXPECT_EQ(6,*i);
	++i;
	EXPECT_EQ(14,*i);
	++i;
	EXPECT_EQ(30,*i);
	++i;
	EXPECT_EQ(62,*i);
	++i;
	EXPECT_EQ(126,*i);
	++i;
	EXPECT_TRUE(i==a.end());
	
	bins c,d;
	c.set(1023);
	d.set(31);
	d.set(32);
	c-=d;
	bins::bin_iterator j = c.begin();
	EXPECT_EQ(33,*j);
}


int main (int argc, char** argv) {
	bin::init();
	bins::init();
	
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
	
}
