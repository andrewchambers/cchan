#include <semaphore.h>
#include <pthread.h>
#include "chan.h"
#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>

void *proc(void *p) {
    Chan *c = p;
    printf("%s\n", (char*)chan_recv(c));
    sleep(1);
    chan_send(c, "pong!");
    return 0;
}

int main() {
    pthread_t thread;
    Chan *c = chan_new();
    if(pthread_create(&thread, NULL, proc, c)) {
        abort();
    }
    chan_send(c, "ping!");
    puts((char*)chan_recv(c));
    return 0;
}
