#!/bin/sh
 
if [ ! $EMIF ] ; then
    exit
fi

if [ ! $EMLOSS ]; then
    EMLOSS=0%
fi

if [ ! $EMDELAY ]; then
    EMDELAY=10ms
fi

if [ ! $EMBW ]; then
    EMBW=10mbit
fi

if [ ! $EMJTTR ]; then
    EMJTTR=0ms
fi

TC="sudo tc "

echo ifb0 up
sudo modprobe ifb
sudo ip link set dev ifb0 up

echo cleanup
$TC qdisc del dev $EMIF ingress
$TC qdisc del dev ifb0 root
 
echo adding ingress
$TC qdisc add dev $EMIF ingress || exit 1

echo redirecting to ifb
$TC filter add dev $EMIF parent ffff: protocol ip prio 1 u32 \
	match ip sport $SWFTPORT 0xffff flowid 1:1 action mirred egress redirect dev ifb0 || exit 2
echo adding netem for $EMDELAY - $EMLOSS
$TC qdisc add dev ifb0 root handle 1:0 netem delay $EMDELAY $EMJTTR 25% loss $EMLOSS || exit 3
echo adding tfb for $EMBW
$TC qdisc add dev ifb0 parent 1:1 handle 10: tbf rate $EMBW buffer 102400 latency 40ms || exit 4

