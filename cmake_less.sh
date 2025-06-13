#!/bin/sh

gcc -o test src/*.c src/C-Thread-Pool/*.c tests/test.c -O0 -lffi -pthread -ggdb3 -fsanitize=address -fsanitize=undefined -ljansson
gcc -o test32 src/*.c src/C-Thread-Pool/*.c tests/test.c -lffi -pthread -m32 -ljansson
./test
./test32
