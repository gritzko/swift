touch .netem-on

sudo tc qdisc add dev eth0 root netem delay 100ms 40ms 25%
