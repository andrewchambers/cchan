set -e
echo test1
gcc -Wfatal-errors -g -pthread chan.c test1.c -o testbin1
timeout 5s ./testbin1
echo test2
gcc -Wfatal-errors -g -pthread chan.c test2.c -o testbin2
timeout 5s ./testbin2
