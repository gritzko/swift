#!/bin/bash

if [ -e ~/.building_swift ]; then
    exit 0
fi

touch ~/.building_swift

if ! which git || ! which g++ || ! which scons || ! which make ; then
    sudo apt-get -y install make g++ scons git-core || exit -4
fi

if [ ! -e ~/include/event.h ]; then
    echo installing libevent
    mkdir tmp
    cd tmp || exit -3
    wget -c http://monkey.org/~provos/libevent-2.0.7-rc.tar.gz || exit -2
    rm -rf libevent-2.0.7-rc
    tar -xzf libevent-2.0.7-rc.tar.gz || exit -1
    cd libevent-2.0.7-rc/ || exit 1
    ./configure --prefix=$HOME || exit 2
    make || exit 3
    make install || exit 4
    cd ~/
    echo done libevent
fi

# if [ ! -e ~/include/gtest/gtest.h ]; then
#     echo installing gtest
#     mkdir tmp
#     cd tmp || exit -3
#     wget -c http://googletest.googlecode.com/files/gtest-1.4.0.tar.bz2 || exit -2
#     rm -rf gtest-1.4.0
#     tar -xjf gtest-1.4.0.tar.bz2 || exit -1
#     cd gtest-1.4.0 || exit 1
#     ./configure --prefix=$HOME || exit 2
#     make || exit 3
#     make install || exit 4
#     cd ~/
#     echo done gtest
# fi

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
echo switching the branch
git checkout $BRANCH || exit 5
echo pulling updates
git pull origin $BRANCH:$BRANCH || exit 5

# echo building
# CPPPATH=~/include LIBPATH=~/lib scons -j4 || exit 7
# echo testing
# tests/connecttest || exit 8

INCLDIR=~/include LIBPATH=~/lib

# TODO: one method
mv bingrep.cpp ext/
if [ ! -e bin ]; then mkdir bin; fi
g++ -I. -I$INCLDIR *.cpp ext/seq_picker.cpp -pg -o bin/swift-pg -L$LIBPATH -levent &
g++ -I. -I$INCLDIR *.cpp ext/seq_picker.cpp -g -o bin/swift-dbg -L$LIBPATH -levent &
g++ -I. -I$INCLDIR *.cpp ext/seq_picker.cpp -O2 -o bin/swift-o2 -L$LIBPATH -levent &
wait

rm ~/.building_swift

echo done
