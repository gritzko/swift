/*
 *  sbittest.cpp
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/9/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#include "bin.h"
#include "sbit.h"
#include <gtest/gtest.h>

TEST(SierpinskyBitmapTest,Init) {
	uint64_t one32 = (1ULL<<32)-1;
	uint64_t mask = one32;
	uint64_t mask_history[5];
	for(int i=0; i<5; i++) {
		mask_history[i] = mask;
		mask = sbit::join(bin(i+1,0),mask,0);
	}
	EXPECT_EQ(1,mask)<<"joined OK";
	for(int i=5; i; i--) {
		uint64pair p = sbit::split(bin(i,0),mask);
		mask = p.first;
		EXPECT_EQ(mask_history[i-1],mask)<<"mask history check";
	}
	EXPECT_EQ(one32,mask)<<"join/split cycle";
	
	EXPECT_EQ(3, sbit::MASKS[0][3])<<"the first two-bit interval, layer 0";
	EXPECT_EQ(5, sbit::MASKS[1][3])<<"the first two-bit interval, layer 1";
	EXPECT_EQ(0x000000000000000FLLU, sbit::MASKS[0][bin(2,0)])<<"the first four-bit interval @0";
	EXPECT_EQ(0xFF00000000000000LLU, sbit::MASKS[0][bin(3,7)])<<"the last eight-bit interval @0";
	EXPECT_EQ(0xAAAA000000000000LLU, sbit::MASKS[1][bin(3,7)])<<"the last eight-bit interval @1";
	EXPECT_EQ(0x8080808080808080LLU, sbit::MASKS[3][bin(3,7)])<<"the last eight-bit interval @3";
	EXPECT_EQ(1<<16, sbit::MASKS[0][bin(0,16)])<< "trivial: bit 16";
	EXPECT_EQ(0x100000000LLU, sbit::MASKS[1][bin(0,16)])<< "trivial: bit 16 layer 1";
	EXPECT_EQ(2, sbit::MASKS[2][bin(0,16)])<< "layout: bit wrap, 16 @layer 2";
}

TEST(SierpinskyBitmapTest,GetSetAggregation) {
	sbit s;
	s.set(3);
	EXPECT_EQ(true,s.get(1)) << "trivial set/retr";
	EXPECT_EQ(false,s.get(31)) << "trivial 0";
	s.set(14);
	s.set(30);
	EXPECT_EQ(false,s.get(31)) << "trivial 0";
	s.set(6);
	EXPECT_EQ(true,s.get(31)) << "aggregated set/retr";
	s.set(127);
	EXPECT_EQ(true,s.get(127)) << "just extension";
	EXPECT_EQ(false,s.get(255)) << "just extension";
	s.set(254);
	EXPECT_EQ(true,s.get(255)) << "aggregated with extension";
}

TEST(SierpinskyBitmapTest,FishEyeBitmaps) {
	sbit s; // 2^8 = 256 bits
	s.set(bin(7,0));
	s.set(bin(5,4));
	s.set(bin(1,128));
	s.set(bin(2,80));
	feye eye(s, bin(6,2));
	// water-ground-air-mountain set/get
	EXPECT_EQ( eye.bits[2], sbit::MASK1 ) << "all-1 chunk";
	EXPECT_EQ( sbit::MASKS[2][bin(0,16)], eye.bits[3] ) << "all-1 chunk";
	EXPECT_EQ( true, eye.get(bin(5,4)) ) << "MOUNTAIN 1-half";
	EXPECT_EQ( false, eye.get(bin(5,5)) ) << "MOUNTAIN 0-half";
	EXPECT_EQ( true, eye.get(bin(6,1)) ) << "MOUNTAIN all-ones";
	EXPECT_EQ( true, eye.get(bin(7,0)) ) << "top-MOUNTAIN 7 layer ones";
	EXPECT_EQ( false, eye.get(bin(7,1)) ) << "AIR just a quarter is 1, must be 0";
	EXPECT_EQ( true, eye.get(bin(2,80)) ) << "GROUND 4-bit fragment is saved";
	EXPECT_EQ( true, eye.get(bin(1,160)) )
		<< "WATER 2-bit fragment is saved (farther from the focus)";
	EXPECT_EQ( false, eye.get(bin(1,128)) ) 
		<< "sunk-WATER 2-bit fragment is lost (far from the focus)";
	// refocus
	feye triv(eye, bin(0,64*3));
	EXPECT_EQ(triv.bits[0],eye.bits[1])<<"trivial refocus 1";
	EXPECT_EQ(triv.bits[1],eye.bits[0])<<"trivial refocus 2";
	EXPECT_EQ(triv.bits[2],eye.bits[2])<<"trivial refocus 3";
	feye ref(eye, bin(0,64*8));
	EXPECT_EQ (false, ref.get(bin(2,80)) ) << "after refocusing, the 4-bit fragment is lost";
	EXPECT_EQ (true, ref.get(bin(7,0)) ) << "but 7l (128bits) fragment is still there";
	EXPECT_EQ (sbit::MASKS[3][bin(4,0)]|sbit::MASKS[3][bin(2,4)],ref.bits[4]) << "gather OK";
	
}


TEST(SierpinskyBitmapTest,BitwiseOps) {
	feye l(bin(6,2));
	feye r(bin(6,2));
	l.set(bin(1,0));
	r.set(bin(1,1));
	feye orf(l);
	orf |= r;
	EXPECT_EQ (true, orf.get(bin(2,0))) << "OR adds up correctly";
	EXPECT_EQ (false, orf.get(bin(2,1))) << "OR adds up correctly";
	feye andf = l&r;
	EXPECT_EQ (false, andf.get(bin(2,0)) ) << "AND adds up correctly";
	feye eye(bin(18,1));
	eye.set(bin::ALL);
	EXPECT_EQ(true,eye.get(bin::ALL)) << "bin_ALL";
	EXPECT_EQ(true,eye.get(bin(12,34))) << "any in bin_ALL";
	EXPECT_EQ(sbit::MASK1,eye.bits[24]) << "bin_ALL: all 1";
}


bin bin_random () {
	return rand()%bin::ALL;
}


TEST (SierpinskyBitmapTest,CryBabyCry) {
	for (int i=0; i<10000; i++) {
		bin point = bin_random();
		bin focus = bin(0,rand()%(1<<30));
		feye f(focus);
		f.set(point);
		bin cp = point.commonParent(focus);
		uint8_t cpheight = cp.layer();
		uint8_t pointheight = point.layer();
		uint8_t topheight = cp==focus? 6 : cpheight-1 ;
		bool visible = topheight-6 <= pointheight;
		EXPECT_EQ (visible, f.get(point)) << 
			"FAIL: another random test: point "<<point<<" focus "<<focus;
		
		bin refocus (0,rand()%(1<<30));
		feye ref(f, refocus);
		bin recp = point.commonParent(refocus);
		uint8_t recpheight = recp.layer();
		uint8_t retopheight = recp==refocus? 6 : recpheight-1 ;
		bool revisible = retopheight-6 <= pointheight;
		EXPECT_EQ (visible&&revisible, ref.get(point))
			<< "FAIL: another random refocus: point "<<point<<
			" focus "<<focus<<" refocus "<<refocus;
		
		if (visible&&revisible) {
			EXPECT_FALSE (pointheight<30 && ref.get(point.parent()))
				<< "FAIL: mystically, parent is 1\n";
			EXPECT_FALSE (pointheight>0 && !ref.get(point.left()))
				<< "FAIL: mystically, left child is 0\n";
			/*EXPECT_FALSE (pointheight>0 && !ref.get(bin_rightn(point,pointheight)))
				<< "FAIL: mystically, the rightmost bit is 0\n";*/
		}
		
	}
	
}


TEST(FishEyeBitmap, OrTest) {
	feye a(bin(0,0)), b(bin(0,256));
	a.set(bin(2,64)); // 256-259
	feye ach(a,bin(0,256));
	EXPECT_TRUE(ach.get(bin(2,64)));
	b.set(bin(0,260));
	b.set(bin(0,261));
	b.set(bin(0,262));
	b.set(bin(0,263));
	EXPECT_TRUE(b.get(bin(2,65)));
	b.refocus(bin(0,257));
	EXPECT_TRUE(b.get(bin(2,65)));
	b |= a;
	EXPECT_TRUE(b.get(bin(2,65).parent()));
}


int main (int argc, char** argv) {
	bin::init();
	sbit::init();
	
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
	
}
