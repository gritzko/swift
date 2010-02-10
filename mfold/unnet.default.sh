if [ -e .netem-on ]; then
    sudo tc qdisc del dev eth0 root
fi
