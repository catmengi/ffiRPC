#!/bin/sh
rm test
rm test32
rm build/ -r
git add *
git commit
git push
