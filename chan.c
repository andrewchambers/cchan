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

static rccondlock* rccondlock_new()  {
    rccondlock *r = xmalloc(sizeof(rccondlock));
    r->rc = 1;
    xmutex_init(&r->l);
    xcond_init(&r->c);
    return r;
}

// Should only be called with lock held.
// Also unlocks the lock.
static void rccondlock_decref(rccondlock *rccl) {
    rccl->rc -= 1;
    if (rccl->rc < 0) {
        fprintf(stderr, "bug - negative refcount\n");
        abort();
    }
    if (rccl->rc == 0) {
        xunlock(&rccl->l);
        xmutex_destroy(&rccl->l);
        xcond_destroy(&rccl->c);
        free(rccl);
        return;
    }
    xunlock(&rccl->l);
}

static void enqueue_blocked(blocked_queue *q, blocked *b) {
    blocked_queue_elem *e = xmalloc(sizeof(blocked_queue_elem));
    if (q->n == 0) {
        q->head = e;
        q->tail = e;
    } else {
        q->tail->next = e;
        q->tail = e;
    }
    q->n += 1;
}

static blocked *dequeue_blocked(blocked_queue *q) {
    if (q->n == 0) {
        return NULL;
    }
    blocked_queue_elem *e = q->head;
    blocked *b = e->b;
    if (q->n == 1) {
        q->head = NULL;
        q->tail = NULL;
        free(e);
    } else {
        q->head = e->next;
    }
    q->n -= 1;
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
    blocked *otherb;
    xlock(&(c->lock));
  again:
    otherb = dequeue_blocked(&c->recvq);
    if (otherb) {
        xlock(&otherb->cl->l);
        if (otherb->cl->done) {
            xunlock(&otherb->cl->l);
            goto again;
        }
        otherb->cl->done = 1;
        *(otherb->outsidx) = otherb->sidx;
        *(otherb->inoutv) = v;
        xcond_broadcast(&otherb->cl->c);
        rccondlock_decref(otherb->cl);
        xunlock(&c->lock);
        return;
    }
    blocked b;
    int donesidx;
    b.cl      = rccondlock_new();
    b.cl->rc  = 2;
    b.outsidx = &donesidx;
    b.inoutv   = &v;
    enqueue_blocked(&c->sendq, &b);
    xunlock(&c->lock);
    while(!(b.cl->done)) {
        xcond_wait(&b.cl->c, &b.cl->l);
    }
    rccondlock_decref(b.cl);
}

static void *chan_recv_unbuff(Chan *c) {
    blocked *otherb;
    void *v;
    xlock(&(c->lock));
  again:
    otherb = dequeue_blocked(&c->sendq);
    if (otherb) {
        xlock(&otherb->cl->l);
        if (otherb->cl->done) {
            xunlock(&otherb->cl->l);
            goto again;
        }
        otherb->cl->done = 1;
        *(otherb->outsidx) = otherb->sidx;
        v = *(otherb->inoutv);
        xcond_broadcast(&otherb->cl->c);
        rccondlock_decref(otherb->cl);
        xunlock(&c->lock);
        return v;
    }
    blocked b;
    int donesidx;
    b.cl      = rccondlock_new();
    b.cl->rc  = 2;
    b.outsidx = &donesidx;
    b.inoutv   = &v;
    enqueue_blocked(&c->recvq, &b);
    xunlock(&c->lock);
    while(!(b.cl->done)) {
        xcond_wait(&b.cl->c, &b.cl->l);
    }
    rccondlock_decref(b.cl);
    return v;
}

void chan_send(Chan *c, void *v) {
    chan_send_unbuff(c, v);
}

void *chan_recv(Chan *c) {
    return chan_recv_unbuff(c);
}

int chan_select(SelectOp so[], int n, int shouldblock) {
    return 0;
}
