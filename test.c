#include <semaphore.h>
#include <pthread.h>
#include "chan.h"
#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>

void *proc1(void *p) {
    Chan *c = p;
    while(1) {
        sleep(1);
        chan_send(c, "ping!");
    }
}

void *proc2(void *p) {
    Chan *c = p;
    sleep(100);
    chan_send(c, "done.");
    return 0;
}

int main() {
    pthread_t thread;
    Chan *c = chan_new(0);
    Chan *d = chan_new(0);
    if(pthread_create(&thread, NULL, proc1, c)) {
        abort();
    }
    if(pthread_create(&thread, NULL, proc2, d)) {
        abort();
    }
    while(1) {
        SelectOp selects[] = {
            {
                .c=c,
                .op=SOP_RECV,
            },
            {
                .c=d,
                .op=SOP_RECV,
            }
        };
        int idx = chan_select(selects, 2, 1);
        switch (idx) {
        case 0:
            puts(selects[0].v);
            break;
        case 1:
            puts("done!");
            return 0;
        default:
            abort();
        }
    }
    return 0;
}
