#!/bin/bash

ulimit -c 1024000
cd swift || exit 2
if [ ! -e ScottKim_2008P.mp4 ]; then
    wget -c http://video.ted.com/talks/podcast/ScottKim_2008P.mp4 || exit 1
fi

(./exec/seeder ScottKim_2008P.mp4 0.0.0.0:$SEEDPORT \
    2> lerr | gzip > lout.gz ) >/dev/null 2> /dev/null &

exit
