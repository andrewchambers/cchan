#include <pthread.h>
#include "chan.h"
#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>

void *proc1(void *p) {
    Chan *c = p;
    while (1) {
        puts("send");
        chan_send(c, "proc1");
    }
    return 0;
}

int main() {
    pthread_t t;
    Chan *a = chan_new(0);
    if (pthread_create(&t, NULL, proc1, a)) {
        abort();
    }
    while (1) {
        sleep(1);
        puts("recv");
        puts((char*)chan_recv(a));
    }
    return 0;
}
