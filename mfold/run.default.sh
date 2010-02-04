#!/bin/bash

HASH=66b9644bb01eaad09269354df00172c8a924773b
HEAD=node300.das2.ewi.tudelft.nl

sleep 1 
ulimit -c 1024000
cd swift || exit 1
rm -f chunk
./exec/leecher $HASH chunk $HEAD:20000 0.0.0.0:10000 >lout 2>lerr || exit 2
