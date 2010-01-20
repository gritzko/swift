#!/bin/bash

if ! which git || ! which g++ || ! which scons || ! which make ; then
    sudo apt-get -y install make g++ scons git-core || exit -4
fi

if [ ! -e ~/include/gtest/gtest.h ]; then
    mkdir tmp
    cd tmp || exit -3
    wget -c http://googletest.googlecode.com/files/gtest-1.4.0.tar.bz2 || exit -2
    rm -rf gtest-1.4.0
    tar -xjf gtest-1.4.0.tar.bz2 || exit -1
    cd gtest-1.4.0 || exit 1
    ./configure --prefix=$HOME || exit 2
    make || exit 3
    make install || exit 4
fi

if [ ! -e swift ]; then
    git clone git://github.com/gritzko/swift.git || exit 6
fi
cd swift
git pull || exit 5

CPPPATH=~/include LIBPATH=~/lib scons -j4 || exit 7
tests/connecttest || exit 8

