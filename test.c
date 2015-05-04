#include <semaphore.h>
#include <pthread.h>
#include "chan.h"
#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>

void *proc1(void *p) {
    Chan *c = p;
    while(1) {
        chan_send(c, "proc1");
    }
    return 0;
}

void *proc2(void *p) {
    Chan *c = p;
    while(1) {
        chan_send(c, "proc2");
    }
    return 0;
}

void *proc3(void *p) {
    Chan *c = p;
    while(1) {
        puts((char*)chan_recv(c));
    }
    return 0;
}

void *proc4(void *p) {
    Chan *c = p;
    while(1) {
        puts((char*)chan_recv(c));
    }
    return 0;
}

int main() {
    pthread_t t;
    Chan *a = chan_new(0);
    Chan *b = chan_new(0);
    Chan *c = chan_new(0);
    Chan *d = chan_new(0);
    if(pthread_create(&t, NULL, proc1, a)) {
        abort();
    }
    if(pthread_create(&t, NULL, proc1, a)) {
        abort();
    }
    if(pthread_create(&t, NULL, proc2, b)) {
        abort();
    }
    if(pthread_create(&t, NULL, proc3, c)) {
        abort();
    }
    if(pthread_create(&t, NULL, proc4, d)) {
        abort();
    }
    if(pthread_create(&t, NULL, proc4, d)) {
        abort();
    }
    while(1) {
        SelectOp selects[] = {
            {
                .c=a,
                .op=SOP_RECV,
            },
            {
                .c=b,
                .op=SOP_RECV,
            },
            {
                .c=c,
                .op=SOP_SEND,
                .v="proc3",
            },
            {
                .c=d,
                .op=SOP_SEND,
                .v="proc4",
            }
        };
        switch (chan_select(selects, 4, 1)) {
        case 0:
            puts(selects[0].v);
            break;
        case 1:
            puts(selects[1].v);
            break;
        case 2:
        case 3:
            break;
        default:
            abort();
        }
        //sleep(1);
    }
    return 0;
}
