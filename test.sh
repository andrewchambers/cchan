set -e
echo test1
gcc -Wfatal-errors -g -pthread chan.c test1.c -o testbin1
if ! timeout 10s ./testbin1 ; then
    echo "fail"
    exit 1
fi
echo test2
gcc -Wfatal-errors -g -pthread chan.c test2.c -o testbin2
if ! timeout 10s ./testbin2 ; then
    echo "fail"
    exit 1
fi
