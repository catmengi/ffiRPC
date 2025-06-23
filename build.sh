#!/bin/sh
mkdir build
cd build
cmake ..
cmake --build .
mv ./tests/donkey ../donkey
cd ..
rm -r build/
