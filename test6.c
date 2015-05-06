#include <pthread.h>
#include "chan.h"
#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

typedef struct  {
    Chan *a;
    Chan *b;
} chanpair;

int sendcnt;
int recvcnt;

void *proc1(void *p) {
    chanpair *cp = p;
    while (1) {
        SelectOp selects[] = {
            {
                .op=SOP_SEND,
                .c=cp->a,
                .v=(void*)0x1337,
            },
            {
                .op=SOP_RECV,
                .c=cp->b,
                .v=0,
            },
        };
        switch (chan_select(selects, 2, 1)) {
        case 0:
            break;
        case 1:
            if ((long long)selects[1].v != 0xdeadbeef) { 
                printf("bad select value proc1 %p\n", selects[1].v);
                abort();
            }
            break;
        default:
            puts("bad select idx");
            abort();
        }
    }
    return 0;
}


int main() {
    pthread_t t;
    Chan *a = chan_new(0);
    Chan *b = chan_new(0);
    chanpair p = {
        a,
        b,
    };
    if (pthread_create(&t, NULL, proc1, &p)) {
        abort();
    }
    long long n = 10000;
    while (n--) {
        SelectOp selects[] = {
            {
                .op=SOP_RECV,
                .c=a,
                .v=0,
            },
            {
                .op=SOP_SEND,
                .c=b,
                .v=(void*)0xdeadbeef,
            }
        };
        switch (chan_select(selects, 2, 1)) {
        case 0:
            if ((long long)selects[0].v != 0x1337) { 
                printf("bad select value main %p\n", selects[0].v);
                abort();
            }
            recvcnt += 1;
            break;
        case 1:
            sendcnt += 1;
            break;
        default:
            puts("bad select idx");
            abort();
        }
    }
    if (sendcnt + recvcnt != 10000 || !sendcnt || !recvcnt) {
        printf("%d %d\n", sendcnt, recvcnt);
        abort();
    }
    return 0;
}
