#include <pthread.h>
#include "chan.h"
#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

pthread_mutex_t gvarmut;
volatile int proc1count = 0;
volatile int proc2count = 0;

void *proc1(void *p) {
    Chan *c = p;
    while (1) {
        usleep(rand() % 1000);
        chan_send(c, (void*)1);
        pthread_mutex_lock(&gvarmut);
        proc1count += 1;
        pthread_mutex_unlock(&gvarmut);
    }
    return 0;
}

void *proc2(void *p) {
    Chan *c = p;
    while (1) {
        usleep(rand() % 1000);
        if ((long long)chan_recv(c) != 2) abort(); 
        pthread_mutex_lock(&gvarmut);
        proc2count += 1;
        pthread_mutex_unlock(&gvarmut);
    }
    return 0;
}

int main() {
    pthread_t t;
    pthread_mutex_init(&gvarmut, NULL);
    Chan *a = chan_new(0);
    Chan *b = chan_new(0);
    if (pthread_create(&t, NULL, proc1, a)) {
        abort();
    }
    if (pthread_create(&t, NULL, proc2, b)) {
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
                .v=(void*)2,
            }
        };
        switch (chan_select(selects, 2, 1)) {
        case 0:
            if ((long long)selects[0].v != 1) { 
                puts("bad select value");
                abort();
            }
            break;
        case 1:
            break;
        default:
            puts("bad select idx");
            abort();
        }
    }
    // Sleep makes sure our counters are updated.
    sleep(1);
    pthread_mutex_lock(&gvarmut);
    if (proc1count + proc2count  != 10000 || proc1count == 0 || proc2count == 0) {
        printf("%d %d %d\n", proc1count, proc2count, proc1count + proc2count);
        abort();
    }
    pthread_mutex_unlock(&gvarmut);
    return 0;
}
