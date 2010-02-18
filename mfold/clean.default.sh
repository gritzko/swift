if [ -e .netem-on ]; then
    sudo tc qdisc del dev `cat .netem-on` root
    rm .netem-on
fi
sudo iptables -F &
cd swift
rm -f chunk core
killall leecher
killall seeder
killall swift-o3
killall swift-dbg
echo DONE
