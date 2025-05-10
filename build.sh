#!/bin/sh
mkdir build
cd build
cmake ..
cmake --build .
valgrind --leak-check=full --show-leak-kinds=all ./tests/donkey
cd ..
rm -r build
