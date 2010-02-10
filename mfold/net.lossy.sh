touch .netem-on

sudo tc qdisc add dev eth0 root netem delay 100ms loss 4.0%
