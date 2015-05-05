#include <pthread.h>
#include "chan.h"
#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

void enqueue_blocked(blocked_queue *q, blocked *b);
blocked *dequeue_blocked(blocked_queue *q);

void test_queue () {
    blocked_queue bq;
    memset(&bq, 0, sizeof(bq));
    int i;
    for (i = 0 ; i < 10 ; i++) {
        if((long long)dequeue_blocked(&bq) != 0) abort();
        enqueue_blocked(&bq, (blocked*)1);
        enqueue_blocked(&bq, (blocked*)2);
        enqueue_blocked(&bq, (blocked*)3);
        enqueue_blocked(&bq, (blocked*)4);
        if((long long)dequeue_blocked(&bq) != 1) abort();
        if((long long)dequeue_blocked(&bq) != 2) abort();
        if((long long)dequeue_blocked(&bq) != 3) abort();
        if((long long)dequeue_blocked(&bq) != 4) abort();
        if((long long)dequeue_blocked(&bq) != 0) abort();
    }
}

pthread_mutex_t gvarmut;
volatile int proc1count = 0;
volatile int proc2count = 0;
volatile int theifcount = 0;

void *proc1(void *p) {
    Chan *c = p;
    while (1) {
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
        chan_send(c, (void*)2);
        pthread_mutex_lock(&gvarmut);
        proc2count += 1;
        pthread_mutex_unlock(&gvarmut);
    }
    return 0;
}

void *theif(void *p) {
    Chan *c = p;
    int n = 15;
    while (n--) {
        chan_recv(c);
        pthread_mutex_lock(&gvarmut);
        theifcount += 1;
        pthread_mutex_unlock(&gvarmut);
    }
    return 0;
}

int main() {
    test_queue();
    pthread_t t;
    pthread_mutex_init(&gvarmut, NULL);
    Chan *a = chan_new(0);
    if (pthread_create(&t, NULL, proc1, a)) {
        abort();
    }
    if (pthread_create(&t, NULL, proc2, a)) {
        abort();
    }
    if (pthread_create(&t, NULL, theif, a)) {
        abort();
    }
    int n = 10000;
    int n1 = 0;
    int n2 = 0;
    while (n--) {
        switch ((long long)chan_recv(a)) {
        case 1:
            n1 += 1;
            break;
        case 2:
            n2 += 1;
            break;
        }
    }
    // Sleep one second to ensure theif thread has died,
    // and that the other two are sleeping.
    sleep(1);
    if (n1 + n2 != 10000) {
        abort();
    }
    pthread_mutex_lock(&gvarmut);
    if (proc1count + proc2count - theifcount != 10000) {
        printf("%d %d %d - %d\n", proc1count, proc2count, theifcount, proc1count + proc2count - theifcount);
        abort();
    }
    pthread_mutex_unlock(&gvarmut);
    return 0;
}
