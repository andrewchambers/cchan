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
        chan_recv(c);
        pthread_mutex_lock(&gvarmut);
        proc1count += 1;
        pthread_mutex_unlock(&gvarmut);
    }
    return 0;
}

void *proc2(void *p) {
    Chan *c = p;
    while (1) {
        chan_recv(c);
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
    if (pthread_create(&t, NULL, proc1, a)) {
        abort();
    }
    if (pthread_create(&t, NULL, proc2, a)) {
        abort();
    }
    int n = 10000;
    while (n--) {
        chan_send(a, (void*)1);
    }
    // Sleep one second to ensure proc counts are correct.
    sleep(1);
    pthread_mutex_lock(&gvarmut);
    if (proc1count + proc2count != 10000) {
        printf("%d %d - %d\n", proc1count, proc2count, proc1count + proc2count);
        abort();
    }
    pthread_mutex_unlock(&gvarmut);
    return 0;
}
