#!/bin/bash

HOST=$1
CMD=$2

if [ -e $CMD.$HOST.sh ] ; then 
    SHSC=$CMD.$HOST.sh ;
else 
    SHSC=$CMD.default.sh ;
fi

if ( cat $SHSC | ssh $HOST ) > .$HOST.$CMD.out 2> .$HOST.$CMD.err; then
    echo $HOST  $CMD    OK
    exit 0
else
    echo $HOST  $CMD    FAIL
    cat .$HOST.$CMD.out .$HOST.$CMD.err
    exit 1
fi
