/*
 *  bintest.cpp
 *  bin++
 *
 *  Created by Victor Grishchenko on 3/9/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "bin.h"
#include <gtest/gtest.h>

TEST(BinTest,Mass) {
    EXPECT_EQ(bin(4).length(),3);
    EXPECT_EQ(bin(9).length(),6);
	EXPECT_EQ(bin(3).mass(),3);
	EXPECT_EQ(bin(5).mass(),1);
	EXPECT_EQ(bin(6).mass(),3);
	EXPECT_EQ(bin(10).mass(),3);
	EXPECT_TRUE(bin::all1(1));
	EXPECT_TRUE(bin::all1(7));	
}

TEST(BinTest,Instantiation) {
	EXPECT_EQ(bin(0,1),2)<<"bin_new @0";
	EXPECT_EQ(bin(2,0),7)<<"bin_new @2";
	EXPECT_EQ(bin(1,2),10)<<"bin_new @1";
	EXPECT_EQ(12,bin(0,7))<<"bin(0,7)";
	EXPECT_EQ(9,bin::tailzeros(512));
	EXPECT_EQ(0,bin::tailzeros(1));
	EXPECT_EQ(5,bin::tailzeros(32+128));
}

TEST(BinTest,Traversing) {
	EXPECT_EQ(6,bin(4).parent());
	EXPECT_EQ(bin(30).parent(),31);
	EXPECT_EQ(bin(7).parent(),15);
	EXPECT_EQ(bin(18).parent(),22);
	EXPECT_EQ(bin(30).left(),22);
	EXPECT_EQ(bin(14).left(),10);
	EXPECT_EQ(bin(22).left(),18);
	EXPECT_EQ(bin(30).right(),29);
	EXPECT_EQ(bin(14).right(),13);
	EXPECT_EQ(bin(22).right(),21);
	EXPECT_EQ(bin(15).left_foot(),1)<<"15 left foot";
	EXPECT_EQ(bin(15).right_foot(),12)<<"15 right foot";
	EXPECT_EQ(bin(22).left_foot(),16)<<"22 left foot";
	EXPECT_EQ(bin(22).right_foot(),20)<<"22 right foot";
}

TEST(BinTest,Advanced) {
	EXPECT_EQ(0,bin(1).layer());
	
	EXPECT_TRUE(bin(31).contains(14));
	EXPECT_FALSE(bin(22).contains(14));
	EXPECT_TRUE(bin(7).contains(7));
	EXPECT_TRUE(bin(11).contains(11));
	EXPECT_TRUE(bin(22).contains(20));
	
	EXPECT_EQ(6,bin(5).commonParent(4)) << "common parent trivial";
	EXPECT_EQ(7,bin(2).commonParent(4)) << "common parent trivial";
	EXPECT_EQ(14,bin(8).commonParent(11)) << "common parent trick";
	EXPECT_EQ(31,bin(14).commonParent(16)) << "common parent complex";
	EXPECT_EQ(31,bin(8).commonParent(16)) << "common parent trick 2";
	EXPECT_EQ(31,bin(31).commonParent(1)) << "common parent trick 2";
	EXPECT_EQ(22,bin(22).commonParent(19)) << "common parent nested";
	EXPECT_EQ(22,bin(19).commonParent(22)) << "common parent nested rev";
	EXPECT_EQ(63,bin(32).commonParent(12)) << "common parent nested rev";
	EXPECT_EQ(31,bin(14).commonParent(18)) << "common parent nested rev";
	EXPECT_EQ(12,bin(12).commonParent(12)) << "common parent nested rev";
	for(bin i=1; i<=127; i++)
		for(bin j=1; j<=127; j++) if (i!=j) {
			bin c = i.commonParent(j);
			EXPECT_TRUE(c.contains(i));
			EXPECT_TRUE(c.contains(j));
			bin l=c.left(), r=c.right();
			//printf("%i %i => %i (%i,%i)\n",(int)i,(int)j,(int)c,(int)l,(int)r);
			EXPECT_FALSE(l.contains(i)&&l.contains(j));
			EXPECT_FALSE(r.contains(i)&&r.contains(j));
		}
	EXPECT_EQ(22,bin(16).parent(2)) << "parent 2";
	EXPECT_EQ(31,bin(9).parent(4)) << "parent-snake";
}

TEST(BinTest,Overflows) {
	// TODO
	//EXPECT_EQ( 1<<31, bin::ALL.length() );
}

TEST(BinTest,Division) {
	EXPECT_EQ(bin(14).modulo(3),14);
	EXPECT_EQ(bin(22).modulo(3),7);
	EXPECT_EQ(bin(21).modulo(1),3);
	EXPECT_EQ(bin(31).modulo(3),15);
	EXPECT_EQ(bin(31).modulo(4),31);
	EXPECT_EQ(bin(22).divide(2),4);
	EXPECT_EQ(bin(30).divide(2),6);
	EXPECT_EQ(bin(31).divide(3),3);
	EXPECT_EQ(bin(14).multiply(1),30);
	EXPECT_EQ(bin(6).multiply(2),30);
}

TEST(BinTest, Scope) {
	EXPECT_EQ(1,bin(32).scoped(bin(62),4));
	EXPECT_EQ(14,bin(29).scoped(bin(30),3));
	EXPECT_EQ(15,bin(30).scoped(bin(30),3));
	EXPECT_EQ(5,bin(11).scoped(bin(31),3));
	EXPECT_EQ(4,bin(22).scoped(bin(31),2));
	
	EXPECT_EQ(14,bin(2).unscoped(bin(15),1));
	EXPECT_EQ(22,bin(4).unscoped(bin(31),2));
}

TEST(BinTest, Order) {
	bin::vec v;
	v.push_back(22);
	v.push_back(17);
	v.push_back(19);
	v.push_back(6);
	v.push_back(3);
	v.push_back(14);
	bin::order(&v);
	ASSERT_EQ(2,v.size());
	EXPECT_EQ(v[1],15);
	EXPECT_EQ(v[0],22);
}

int main (int argc, char** argv) {
	bin::init();
	
	bin p(12234);
	printf("%i %i %i %i\n",(int)p,p.layer(),p.offset(),p.offset()<<p.layer());
	p = 16382;
	printf("%i %i %i %i\n",(int)p,p.layer(),p.offset(),p.offset()<<p.layer());
	
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
	
}
