#include <pthread.h>
#include <semaphore.h>
#include "chan.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void *xmalloc(size_t sz) {
    void *m = malloc(sz);
    if (!m) {
        abort();
    }
    memset(m, 0, sz);
    return m;
}

static void *xrealloc(void *p, size_t sz) {
    p = realloc(p, sz);
    if (!p) {
        abort();
    }
    return p;
}

static void xsem_init(sem_t *s, int v) {
    if (sem_init(s, 0, v)) {
        abort();
    }
}

static void xsem_post(sem_t *s) {
    if (sem_post(s)) {
        abort();
    }
}

static void xsem_wait(sem_t *s) {
    if (sem_wait(s)) {
        abort();
    }
}

static void xsem_destroy(sem_t *s) {
    if (sem_destroy(s)) {
        abort();
    }
}

static void xcond_init(pthread_cond_t *c) {
    if (pthread_cond_init(c, NULL)) {
        abort();
    }
}

static void xcond_destroy(pthread_cond_t *c) {
    if (pthread_cond_destroy(c)) {
        abort();
    }
}

static void xcond_broadcast(pthread_cond_t *c) {
    if (pthread_cond_broadcast(c)) {
        abort();
    }
}

static void xmutex_init(pthread_mutex_t *m) {
    if (pthread_mutex_init(m, NULL)) {
        abort();
    }
}

static void xmutex_destroy(pthread_mutex_t *m) {
    if (pthread_mutex_destroy(m)) {
        abort();
    }
}

static void xlock(pthread_mutex_t *m) {
    if (pthread_mutex_lock(m)) {
        abort();
    }
}

static void xunlock(pthread_mutex_t *m) {
    if (pthread_mutex_unlock(m)) {
        abort();
    }
}

static void xcond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    if (pthread_cond_wait(c, m)) {
        abort();
    }
}


static blocked *blocked_new() {
    blocked *b = xmalloc(sizeof(blocked));
    xmutex_init(&(b->lock));
    xcond_init(&(b->cond));
    return b;
}

static void blocked_free(blocked *b) {
    xmutex_destroy(&(b->lock));
    xcond_destroy(&(b->cond));
    free(b);
}

static void bladd(blocked_list *bl, blocked *b) {
    if (bl->n + 1 >= bl->sz) {
        bl->pblocked = xrealloc(bl->pblocked, bl->sz + 100 * sizeof(blocked*));
        bl->sz += 100;
    }
    bl->pblocked[bl->n] = b;
    bl->n += 1;
}

static blocked *bltake(blocked_list *bl) {
    if (bl->n < 1) {
        abort();
    }
    int idx = ((unsigned int)rand()) % bl->n;
    blocked *b = bl->pblocked[idx];
    bl->pblocked[idx] = bl->pblocked[bl->n - 1];
    bl->pblocked[bl->n - 1] = 0;
    bl->n -= 1;
    return b;
}

Chan *chan_new(int sz) {
    Chan *c = xmalloc(sizeof(Chan));
    xmutex_init(&(c->lock));
    xcond_init(&(c->cond));
    if (sz < 0) {
        abort();
    }
    c->buffsz = sz;
    if (sz) {
        c->vbuff = xmalloc(sizeof(void*) * sz);
    }
    return c;
}

void chan_free(Chan *c) {
    xmutex_destroy(&(c->lock));
    xcond_destroy(&(c->cond));
    // XXX blocked lists
    free(c);
}

void chan_close(Chan *c) {

}

static void chan_send_buff(Chan *c, void *v) {
    xlock(&(c->lock));
    while (c->nbuff == c->buffsz) {
        xcond_wait(&(c->cond), &(c->lock));
    }
    c->vbuff[c->nbuff] = v;
    c->nbuff += 1;
    c->bend = (c->bend + 1) % c->buffsz;
    xcond_broadcast(&(c->cond));
    xunlock(&(c->lock));
}

static void *chan_recv_buff(Chan *c) {
    void *v = 0;
    xlock(&(c->lock));
    while (c->nbuff == 0) {
        xcond_wait(&(c->cond), &(c->lock));
    }
    v = c->vbuff[c->bstart];
    c->vbuff[c->bstart] = 0;
    c->bstart += 1;
    c->nbuff -= 1;
    xcond_broadcast(&(c->cond));
    xunlock(&(c->lock));
    return v;
}

static void chan_send_unbuff(Chan *c, void *v) {
    xlock(&(c->lock));
    if (c->receivers.n){
        blocked *b = bltake(&(c->receivers));
        xlock(&(b->lock));
        b->v = v;
        b->done = 1;
        xcond_broadcast(&(b->cond));
        xunlock(&(b->lock));
        xunlock(&(c->lock));
        return;
    }
    blocked *b = blocked_new();
    b->v = v;
    bladd(&(c->senders), b);
    xlock(&(b->lock));
    xunlock(&(c->lock));
    while (!b->done) {
        xcond_wait(&(b->cond), &(b->lock));
    }
    xunlock(&(b->lock));
    blocked_free(b);
}

static void *chan_recv_unbuff(Chan *c) {
    void *v = 0;
    xlock(&(c->lock));
    if (c->senders.n){
        blocked *b = bltake(&(c->senders));
        xlock(&(b->lock));
        v = b->v;
        b->done = 1;
        xcond_broadcast(&(b->cond));
        xunlock(&(b->lock));
        xunlock(&(c->lock));
        return v;
    }
    blocked *b = blocked_new();
    bladd(&(c->receivers), b);
    xlock(&(b->lock));
    xunlock(&(c->lock));
    while (!b->done) {
        xcond_wait(&(b->cond), &(b->lock));
    }
    v = b->v;
    xunlock(&(b->lock));
    blocked_free(b);
    return v;
}

void chan_send(Chan *c, void *v) {
    if (!c->buffsz) {
        chan_send_unbuff(c, v);
    } else {
        chan_send_buff(c, v);
    }
}

void *chan_recv(Chan *c) {
    if (!c->buffsz) {
        return chan_recv_unbuff(c);
    } else {
        return chan_recv_buff(c);
    }
}

int chan_select(SelectOp so[], int n, int shouldblock) {
    if (n < 1) {
        abort();
    }
    // Random start index to avoid starvation.
    int sidx = rand() % n;
    int idx;
    while(1) {
        int i;
        for (i = 0; i < n ; i++) {
            idx = (sidx + i) % n;
            SelectOp *curop = &so[idx];
            Chan *c = curop->c;
            int lockrc = pthread_mutex_trylock(&(c->lock));
            if (lockrc == EBUSY) {
                continue;
            }
            if (lockrc) {
                abort();
            }
            switch (curop->op) {
            case SOP_RECV:
                if (c->buffsz == 0) {
                    if (c->senders.n){
                        blocked *b = bltake(&(c->senders));
                        xlock(&(b->lock));
                        curop->v = b->v;
                        b->done = 1;
                        xcond_broadcast(&(b->cond));
                        xunlock(&(b->lock));
                        xunlock(&(c->lock));
                        printf("s %d\n", idx);
                        return idx;
                    }
                } else {
                    if (c->nbuff > 0) {
                        curop->v = c->vbuff[c->bstart];
                        c->vbuff[c->bstart] = 0;
                        c->bstart += 1;
                        c->nbuff -= 1;
                        xcond_broadcast(&(c->cond));
                        xunlock(&(c->lock));
                        return idx;
                    }
                }
                break;
            case SOP_SEND:
                if (c->buffsz == 0) {
                    if (c->receivers.n){
                        blocked *b = bltake(&(c->receivers));
                        xlock(&(b->lock));
                        b->v = curop->v;
                        b->done = 1;
                        xcond_broadcast(&(b->cond));
                        xunlock(&(b->lock));
                        xunlock(&(c->lock));
                        return idx;
                    }
                } else {
                    if (c->nbuff < c->buffsz) {
                        c->vbuff[c->nbuff] = curop->v;
                        c->nbuff += 1;
                        c->bend = (c->bend + 1) % c->buffsz;
                        xcond_broadcast(&(c->cond));
                        xunlock(&(c->lock));
                        return idx;
                    }
                }
                break;
            default:
                abort();
            }
            xunlock(&(c->lock));
        }
        // XXX we need some way of waking up the select.
        // Could use a global condvar + broadcast on 
        // channel ops?
        usleep(10000);
        if (!shouldblock) {
            return -1;
        }
    }
}
