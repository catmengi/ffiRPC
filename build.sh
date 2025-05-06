#!/bin/sh
mkdir build
cd build
cmake ..
cmake --build .
valgrind --leak-check=full ./tests/donkey
cd ..
rm -r build
