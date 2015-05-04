#include <pthread.h>
#include <semaphore.h>
#include "chan.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>

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

static void blocked_decref(blocked *b) {
    b->refcount--;
    int rc = b->refcount;
    xunlock(&(b->lock));
    if (rc == 0) {
        xmutex_destroy(&(b->lock));
        xcond_destroy(&(b->cond));
        free(b);
    } else {
    }
}

static void bladd(blocked_list *bl, blocked *b, int sidx) {
    if (bl->n + 1 >= bl->sz) {
        bl->pblocked = xrealloc(bl->pblocked, (bl->sz + 32) * sizeof(blocked*));
        bl->sindex = xrealloc(bl->sindex, (bl->sz + 32) * sizeof(int));
        bl->sz += 32;
    }
    bl->pblocked[bl->n] = b;
    bl->sindex[bl->n] = sidx;
    bl->n += 1;
}

static blocked *bltake(blocked_list *bl, int *poutsidx) {
    if (bl->n < 1) {
        abort();
    }
    int idx = ((unsigned int)rand()) % bl->n;
    blocked *b = bl->pblocked[idx];
    int outsidx = bl->sindex[idx];
    bl->pblocked[idx] = bl->pblocked[bl->n - 1];
    bl->pblocked[bl->n - 1] = 0;
    bl->sindex[idx] = bl->sindex[bl->n - 1];
    bl->sindex[bl->n - 1] = 0;
    bl->n -= 1;
    if (bl->n == 0) {
        // The blocked list is freed automatically when the last 
        // blocked item is unlocked.
        free(bl->pblocked);
        free(bl->sindex);
        bl->sindex = 0;
        bl->pblocked = 0;
        bl->sz = 0;
    }
    *poutsidx = outsidx;
    return b;
}

Chan *chan_new(int sz) {
    Chan *c = xmalloc(sizeof(Chan));
    xmutex_init(&(c->lock));
    if (sz != 0) {
        abort();
    }
    return c;
}

void chan_free(Chan *c) {
    xmutex_destroy(&(c->lock));
    free(c);
}

void chan_close(Chan *c) {

}

static void chan_send_unbuff(Chan *c, void *v) {
    xlock(&(c->lock));
  again:
    if (c->receivers.n){
        int sidx;
        blocked *b = bltake(&(c->receivers), &sidx);
        xlock(&(b->lock));
        int rc = b->refcount;
        if (b->done) {
            xcond_broadcast(&(b->cond));
            blocked_decref(b);
            goto again; 
        }
        b->v = v;
        b->done = 1;
        b->outsidx = sidx;
        xcond_broadcast(&(b->cond));
        blocked_decref(b);
        xunlock(&(c->lock));
        return;
    }
    blocked *b = blocked_new();
    b->refcount = 2;
    b->v = v;
    bladd(&(c->senders), b, -1);
    xlock(&(b->lock));
    xunlock(&(c->lock));
    while (!b->done) {
        xcond_wait(&(b->cond), &(b->lock));
    }
    xcond_broadcast(&(b->cond));
    blocked_decref(b);
}

static void *chan_recv_unbuff(Chan *c) {
    void *v = 0;
    xlock(&(c->lock));
  again:
    if (c->senders.n){
        int sidx;
        blocked *b = bltake(&(c->senders), &sidx);
        xlock(&(b->lock));
        int rc = b->refcount;
        if (b->done) {
            blocked_decref(b);
            goto again; 
        }
        v = b->v;
        b->done = 1;
        b->outsidx = sidx;
        xcond_broadcast(&(b->cond));
        blocked_decref(b);
        xunlock(&(c->lock));
        return v;
    }
    blocked *b = blocked_new();
    b->refcount = 2;
    bladd(&(c->receivers), b, -1);
    xlock(&(b->lock));
    xunlock(&(c->lock));
    while (!b->done) {
        xcond_wait(&(b->cond), &(b->lock));
    }
    v = b->v;
    xcond_broadcast(&(b->cond));
    blocked_decref(b);
    return v;
}

void chan_send(Chan *c, void *v) {
    chan_send_unbuff(c, v);
}

void *chan_recv(Chan *c) {
    return chan_recv_unbuff(c);
}

int chan_select(SelectOp so[], int n, int shouldblock) {
    if (n < 1) {
        abort();
    }
    // Random start index to avoid starvation.
    blocked *selectb = blocked_new();
    selectb->refcount = 1;
    xlock(&(selectb->lock));
    int i;
    int startidx = rand() % n;
    for (i = 0; i < n ; i++) {
        int idx = (startidx + i) % n;
        SelectOp *curop = &so[idx];
        Chan *c = curop->c;
        xlock(&(c->lock));
        switch (curop->op) {
        case SOP_RECV:
        recvagain:
            if (c->senders.n){
                int sidx;
                blocked *b = bltake(&(c->senders), &sidx);
                xlock(&(b->lock));
                if (b->done) {
                    xcond_broadcast(&(b->cond));
                    blocked_decref(b);
                    goto recvagain; 
                }
                curop->v = b->v;
                b->done = 1;
                b->outsidx = sidx;
                selectb->done = 1;
                xcond_broadcast(&(b->cond));
                xcond_broadcast(&(selectb->cond));
                blocked_decref(selectb);
                blocked_decref(b);
                xunlock(&(c->lock));
                return idx;
            }
            selectb->refcount++;
            bladd(&(c->receivers), selectb, idx);
            break;
        case SOP_SEND:
            sendagain:
            if (c->receivers.n){
                void *v;
                int sidx;
                blocked *b = bltake(&(c->receivers), &sidx);
                xlock(&(b->lock));
                if (b->done) {
                    xcond_broadcast(&(b->cond));
                    blocked_decref(b);
                    goto sendagain; 
                }
                b->v = curop->v;
                b->done = 1;
                b->outsidx = sidx;
                selectb->done = 1;
                xcond_broadcast(&(b->cond));
                xcond_broadcast(&(selectb->cond));
                blocked_decref(selectb);
                blocked_decref(b);
                xunlock(&(c->lock));
                return idx;
            }
            selectb->refcount++;
            bladd(&(c->senders), selectb, idx);
            break;
        default:
            abort();
        }
        xunlock(&(c->lock));
    }
    while (!selectb->done) {
        xcond_wait(&(selectb->cond), &(selectb->lock));
    }
    // For send this doesn't matter.
    // For recv this gets the value.
    so[selectb->outsidx].v = selectb->v;
    int ridx = selectb->outsidx;
    blocked_decref(selectb);
    return ridx;
}
