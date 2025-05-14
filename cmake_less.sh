#!/bin/sh

gcc -o test src/*.c src/C-Thread-Pool/*.c tests/test.c -lffi -pthread -ggdb3 -fsanitize=address
./test
