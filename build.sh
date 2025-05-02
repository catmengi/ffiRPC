#!/bin/sh
mkdir build
cd build
cmake ..
cmake --build .
./tests/donkey
cd ..
rm -r build
