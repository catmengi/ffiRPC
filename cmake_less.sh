#!/bin/sh

gcc -o test -I./include/ src/*.c src/C-Thread-Pool/*.c tests/test.c -O0 -lffi -pthread -ggdb3 -fsanitize=address -fsanitize=undefined -ljansson
gcc -o valgrind_test -pg -I./include/ src/*.c src/C-Thread-Pool/*.c tests/test.c -O0 -lffi -pthread -ggdb3 -ljansson
gcc -o test32 -I./include/ src/*.c src/C-Thread-Pool/*.c tests/test.c -lffi -pthread -m32 -ljansson
./test
./test32
valgrind --leak-check=full ./valgrind_test
