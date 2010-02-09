#!/bin/bash

HASH=66b9644bb01eaad09269354df00172c8a924773b
HEAD=83.96.143.114

sleep 1 
rm -f core
ulimit -c 1024000
cd swift || exit 1
rm -f chunk
#valgrind --leak-check=yes \
./exec/leecher $HASH chunk $HEAD:10001 0.0.0.0:10002 2>lerr | gzip > lout.gz || exit 2
