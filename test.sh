set -e

for T in `ls test*.c` 
do
    echo $T
    clang -fsanitize=thread -Wfatal-errors -g -pthread chan.c $T -o $T.bin            
    if ! timeout 30s ./$T.bin ; then
        echo "fail"
        exit 1
    fi
done
