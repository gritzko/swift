cd swift
rm -f chunk core lout lerr
killall leecher
killall seeder
if [ -e .netem-iface ]; then
    sudo tc qdisc del dev `cat .netem-iface` root
    rm .netem-iface
fi
echo DONE
