#!/bin/sh

gcc -o test src/* tests/test.c -lffi -pthread -g -fsanitize=address
./test
