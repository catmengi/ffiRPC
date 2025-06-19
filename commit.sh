#!/bin/sh
rm test
rm test32
rm valgrind_test
rm build/ -r
git add *
git commit
git push
