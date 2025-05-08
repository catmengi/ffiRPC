#!/bin/sh

gcc -o test src/* tests/test.c -lffi -pthread -ggdb3 -fsanitize=address
./test
