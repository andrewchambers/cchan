set -e
gcc -g -pthread chan.c test.c -o testbin
./testbin