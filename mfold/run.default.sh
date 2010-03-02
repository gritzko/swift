#!/bin/bash
# This script runs a leecher at some server;
# env variables are set in env.default.sh

ulimit -c 1024000
cd swift || exit 1
rm -f core
rm -f chunk
sleep $(( $RANDOM % 5 ))
bin/swift-o2 -w -h $HASH -f chunk -t $SEEDER:$SWFTPORT \
    -l 0.0.0.0:$RUNPORT -p -D 2>lerr | gzip > lout.gz || exit 2
