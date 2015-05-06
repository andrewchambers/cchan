#include <pthread.h>
#include "chan.h"
#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

pthread_mutex_t gvarmut;
volatile int proc1count = 0;
volatile int proc2count = 0;
volatile int proc3count = 0;

void *proc1(void *p) {
    Chan *c = p;
    while (1) {
        usleep(rand() % 1000);
        if((long long)chan_recv(c) != 1) abort();
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
        if((long long)chan_recv(c) != 2) abort();
        pthread_mutex_lock(&gvarmut);
        proc2count += 1;
        pthread_mutex_unlock(&gvarmut);
    }
    return 0;
}

void *proc3(void *p) {
    Chan *c = p;
    while (1) {
        usleep(rand() % 1000);
        if((long long)chan_recv(c) != 3) abort();
        pthread_mutex_lock(&gvarmut);
        proc3count += 1;
        pthread_mutex_unlock(&gvarmut);
    }
    return 0;
}

int main() {
    pthread_t t;
    pthread_mutex_init(&gvarmut, NULL);
    Chan *a = chan_new(0);
    Chan *b = chan_new(0);
    Chan *c = chan_new(0);
    if (pthread_create(&t, NULL, proc1, a)) {
        abort();
    }
    if (pthread_create(&t, NULL, proc2, b)) {
        abort();
    }
    if (pthread_create(&t, NULL, proc3, c)) {
        abort();
    }
    long long n = 10000;
    while (n--) {
        int sidx;
        SelectOp selects[] = {
            {
                .op=SOP_SEND,
                .c=a,
                .v=(void*)1,
            },
            {
                .op=SOP_SEND,
                .c=b,
                .v=(void*)2,
            },
            {
                .op=SOP_SEND,
                .c=c,
                .v=(void*)3,
            },
        };
        sidx = chan_select(selects, 3, 1);
        //printf("sidx %d\n", sidx);
    }
    // Sleep makes sure our counters are updated.
    sleep(1);
    pthread_mutex_lock(&gvarmut);
    if (proc1count + proc2count + proc3count != 10000 || proc1count == 0 || proc2count == 0 || proc3count == 0) {
        printf("%d %d %d %d\n", proc1count, proc2count, proc3count, proc1count + proc2count + proc3count);
        abort();
    }
    // printf("%d %d %d %d\n", proc1count, proc2count, proc3count, proc1count + proc2count + proc3count);
    pthread_mutex_unlock(&gvarmut);
    return 0;
}
