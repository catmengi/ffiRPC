#!/bin/sh

gcc -o test src/*.c src/C-Thread-Pool/*.c tests/test.c -lffi -pthread -ggdb3 -fsanitize=address -fsanitize=undefined
gcc -o test32 src/*.c src/C-Thread-Pool/*.c tests/test.c -lffi -pthread -m32
./test
./test32
