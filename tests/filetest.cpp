#include "file.h"
#include <gtest/gtest.h>

TEST(FileTest,mmap) {
    // open
    // mmap
    // unmap
    // mmap
    // read
    // close
    // open
    // read fails
    // mmap
    // read
    // close
}

TEST(FileTest,retrieval) {
    // create with a root hash
    // supply with hashes and data
    // check peak hashes
    // one broken packet
    // check history
    // close
    // verify
}

TEST(FileTest,Streaming) {
}

int main (int argc, char** argv) {

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
	
}
