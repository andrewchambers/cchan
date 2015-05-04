set -e
gcc -Wfatal-errors -g -pthread chan.c test.c -o testbin
./testbin