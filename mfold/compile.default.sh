cd swift || exit 1
git pull || exit 2
rm exec/*-pg exec/*-o3

g++ -I. exec/leecher.cpp *.cpp compat/*.cpp ext/seq_picker.cpp -pg -o exec/leecher-pg &
g++ -I. exec/seeder.cpp *.cpp compat/*.cpp ext/seq_picker.cpp -pg -o exec/seeder-pg &
g++ -I. exec/leecher.cpp *.cpp compat/*.cpp ext/seq_picker.cpp -O3 -o exec/leecher-o3 &
g++ -I. exec/seeder.cpp *.cpp compat/*.cpp ext/seq_picker.cpp -O3 -o exec/seeder-o3 &

wait

cd exec

if [ ! -e leecher-pg ]; then exit 3; fi
if [ ! -e leecher-o3 ]; then exit 4; fi
if [ ! -e seeder-pg ]; then exit 5; fi
if [ ! -e seeder-o3 ]; then exit 6; fi

