#!/bin/bash

if ! which git || ! which g++ || ! which scons || ! which make ; then
    sudo apt-get -y install make g++ scons git-core || exit -4
fi

if [ ! -e ~/include/gtest/gtest.h ]; then
    echo installing gtest
    mkdir tmp
    cd tmp || exit -3
    wget -c http://googletest.googlecode.com/files/gtest-1.4.0.tar.bz2 || exit -2
    rm -rf gtest-1.4.0
    tar -xjf gtest-1.4.0.tar.bz2 || exit -1
    cd gtest-1.4.0 || exit 1
    ./configure --prefix=$HOME || exit 2
    make || exit 3
    make install || exit 4
    echo done gtest
fi

#if ! which pcregrep ; then
#    echo installing pcregrep
#    mkdir tmp
#    cd tmp
#    wget ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre-8.01.tar.gz || exit 5
#    tar -xzf pcre-8.01.tar.gz 
#    cd pcre-8.01
#    ./configure --prefix=$HOME || exit 6
#    make -j4 || exit 7
#    make install || exit 8
#    echo done pcregrep
#fi

if [ ! -e swift ]; then
    echo clone the repo
    git clone $ORIGIN || exit 6
fi
cd swift
echo pulling updates
git pull origin $BRANCH:$BRANCH || exit 5
echo switching the branch
git checkout $BRANCH || exit 5

echo building
CPPPATH=~/include LIBPATH=~/lib scons -j4 || exit 7
echo testing
tests/connecttest || exit 8

# TODO: one method
if [ ! -e bin ]; then mkdir bin; fi
g++ -I. *.cpp ext/seq_picker.cpp -pg -o bin/swift-pg &
g++ -I. *.cpp ext/seq_picker.cpp -g -o bin/swift-dbg &
g++ -I. *.cpp ext/seq_picker.cpp -O2 -o bin/swift-o2 &
wait

echo done
