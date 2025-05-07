#!/bin/sh
rm test
rm build/ -r
git add *
git commit
git push
