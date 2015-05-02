#include <semaphore.h>
#include <pthread.h>
#include "chan.h"
#include "stdlib.h"
#include <stdio.h>
#include <unistd.h>

void *proc(void *p) {
    Chan *c = p;
    int i;
    for (i = 0 ; i < 3; i++) {
        printf("%s\n", (char*)chan_recv(c));
        sleep(1);
        chan_send(c, "pong!");
    }
    chan_recv(c);
    chan_send(c, "done!");
    return 0;
}

int main() {
    pthread_t thread;
    Chan *c = chan_new(0);
    if(pthread_create(&thread, NULL, proc, c)) {
        abort();
    }
    while(1) {
        chan_send(c, "ping!");
        char *r = chan_recv(c);
        printf("%s\n", r);
        if (strcmp(r, "done!") == 0) {
            break;
        }
    }
    return 0;
}
