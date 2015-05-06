#include <pthread.h>
#include <semaphore.h>
#include "chan.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>

static void *xmalloc(size_t sz) {
    void *m = malloc(sz);
    if (!m) {
        fputs("xcond_malloc\n", stderr);
        abort();
    }
    memset(m, 0, sz);
    return m;
}

static void xcond_init(pthread_cond_t *c) {
    if (pthread_cond_init(c, NULL)) {
        fputs("xcond_init\n", stderr);
        abort();
    }
}

static void xcond_destroy(pthread_cond_t *c) {
    if (pthread_cond_destroy(c)) {
        fputs("xcond_destroy\n", stderr);
        abort();
    }
}

static void xcond_broadcast(pthread_cond_t *c) {
    if (pthread_cond_broadcast(c)) {
        fputs("xcond_broadcast\n", stderr);
        abort();
    }
}

static void xmutex_init(pthread_mutex_t *m) {
    if (pthread_mutex_init(m, NULL)) {
        fputs("xmutex_init\n", stderr);
        abort();
    }
}

static void xmutex_destroy(pthread_mutex_t *m) {
    if (pthread_mutex_destroy(m)) {
        fputs("xmutex_destroy\n", stderr);
        abort();
    }
}

static void xlock(pthread_mutex_t *m) {
    if (pthread_mutex_lock(m)) {
        fputs("xlock\n", stderr);
        abort();
    }
}

static void xunlock(pthread_mutex_t *m) {
    if (pthread_mutex_unlock(m)) {
        fputs("xunlock\n", stderr);
        abort();
    }
}

static void xcond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
    if (pthread_cond_wait(c, m)) {
        fputs("xcond_wait\n", stderr);
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
    // DO NOT READ RC WITHOUT LOCK.
    if (rccl->rc == 0) {
        xunlock(&rccl->l);
        xmutex_destroy(&rccl->l);
        xcond_destroy(&rccl->c);
        free(rccl);
        return;
    }
    xunlock(&rccl->l);
}

void enqueue_blocked(blocked_queue *q, blocked b) {
    blocked_queue_elem *e = xmalloc(sizeof(blocked_queue_elem));
    e->b = b;
    if (q->n == 0) {
        q->head = e;
        q->tail = e;
    } else {
        q->tail->next = e;
        q->tail = e;
    }
    q->n += 1;
}

int dequeue_blocked(blocked_queue *q, blocked *out) {
    if (q->n == 0) {
        return 0;
    }
    blocked_queue_elem *e = q->head;
    *out = e->b;
    if (q->n == 1) {
        q->head = NULL;
        q->tail = NULL;
    } else {
        q->head = e->next;
    }
    free(e);
    q->n -= 1;
    return 1;
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
    blocked otherb;
    xlock(&(c->lock));
  again:
    if (dequeue_blocked(&c->recvq, &otherb)) {
        xlock(&otherb.cl->l);
        if (otherb.cl->done) {
            rccondlock_decref(otherb.cl);
            goto again;
        }
        otherb.cl->done = 1;
        otherb.cl->outsidx = otherb.sidx;
        *(otherb.inoutv) = v;
        xunlock(&c->lock);
        xcond_broadcast(&otherb.cl->c);
        rccondlock_decref(otherb.cl);
        return;
    }
    blocked b;
    b.cl      = rccondlock_new();
    b.cl->rc  = 2;
    b.sidx    = -1;
    b.inoutv  = &v;
    enqueue_blocked(&c->sendq, b);
    xunlock(&c->lock);
    xlock(&b.cl->l);
    while(!(b.cl->done)) {
        xcond_wait(&b.cl->c, &b.cl->l);
    }
    rccondlock_decref(b.cl);
}

static void *chan_recv_unbuff(Chan *c) {
    blocked otherb;
    void *v;
    xlock(&(c->lock));
  again:
    if (dequeue_blocked(&c->sendq, &otherb)) {
        xlock(&otherb.cl->l);
        if (otherb.cl->done) {
            rccondlock_decref(otherb.cl);
            goto again;
        }
        otherb.cl->done = 1;
        otherb.cl->outsidx = otherb.sidx;
        v = *(otherb.inoutv);
        xunlock(&c->lock);
        xcond_broadcast(&otherb.cl->c);
        rccondlock_decref(otherb.cl);
        return v;
    }
    blocked b;
    b.cl      = rccondlock_new();
    b.cl->rc  = 2;
    b.sidx    = -1;
    b.inoutv  = &v;
    enqueue_blocked(&c->recvq, b);
    xunlock(&c->lock);
    xlock(&b.cl->l);
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
    int i;
    blocked b;
    b.cl      = rccondlock_new();
    b.cl->rc  = 1;
    xlock(&b.cl->l);
    // random start index to fairly select.
    int startidx = rand() % n;
    for (i = 0; i < n; i++) {
        int idx = (i + startidx) % n;
        SelectOp *cop = &so[idx];
        Chan *c = cop->c;
        switch (cop->op) {
        case SOP_SEND: {
            blocked otherb;
            xlock(&(c->lock));
          sendagain:
            if (dequeue_blocked(&c->recvq, &otherb)) {
                xlock(&otherb.cl->l);
                if (otherb.cl->done) {
                    rccondlock_decref(otherb.cl);
                    goto sendagain;
                }
                b.cl->done = 1;
                otherb.cl->done = 1;
                otherb.cl->outsidx = otherb.sidx;
                *(otherb.inoutv) =  cop->v;
                xunlock(&c->lock);
                xcond_broadcast(&otherb.cl->c);
                rccondlock_decref(otherb.cl);
                rccondlock_decref(b.cl);
                return idx;
            }
            b.cl->rc += 1;
            b.sidx    = idx;
            b.inoutv  = &cop->v;
            enqueue_blocked(&c->sendq, b);
            xunlock(&c->lock);
            break;
        }
        case SOP_RECV: {

        }
        default:
            abort();
        }
    }
    while (!b.cl->done) {
        xcond_wait(&b.cl->c, &b.cl->l);
    }
    int donesidx = b.cl->outsidx;
    rccondlock_decref(b.cl);
    return donesidx;
}


