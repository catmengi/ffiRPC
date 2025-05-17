#!/bin/sh
mkdir build
cd build
cmake ..
cmake --build .
./tests/donkey
#valgrind --main-stacksize=10000000 --leak-check=full --show-leak-kinds=all ./tests/donkey
cd ..
rm -r build
