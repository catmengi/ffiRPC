gcc -o O3 -O3 -I./include/ src/*.c src/C-Thread-Pool/*.c tests/test.c -O0 -lffi -pthread -ggdb3 -ljansson
time ./O3
