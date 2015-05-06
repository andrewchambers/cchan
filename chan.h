
// A C implementation of CSP channels similar to Go.
//
// Channels are a conduit for which syncrhonizing and
// communicating between multiple threads.
//
// Sending of recieving may block if the channels buffer is full.
// 
// 
// To use put before this header.
// #include <pthread.h> 

// Chan represents a CSP channel.
// Full definition is in chan.c
// The internals of the channel are private.

// ref counted lock set.
typedef struct {
    int rc;
    volatile int done;
    volatile int outsidx;
    pthread_cond_t  c;
    pthread_mutex_t l;
} rccondlock;

typedef struct {
    rccondlock *cl;
    // The select index which wrote wrote the queue entry.
    int sidx;
    // Either the value to read, or the place to write the value.
    void **inoutv;
} blocked;

typedef struct blocked_queue_elem {
    struct blocked_queue_elem *next;
    blocked b;
} blocked_queue_elem;

typedef struct {
    int n;
    blocked_queue_elem *head;
    blocked_queue_elem *tail;
} blocked_queue;

typedef struct {
    pthread_mutex_t lock;
    blocked_queue   sendq;
    blocked_queue   recvq;
} Chan;

// chan_sop is an enum defined for use with chan_select.
enum chan_sop {
    SOP_RECV,
    SOP_SEND,
};

typedef struct {
    enum chan_sop op;
    void          *v;
    Chan          *c;
} SelectOp;

// Return a new open channel
Chan *chan_new(int sz);

// Free a channel and any associated resource.
// will not free any items still in the channels buffer.
void chan_free(Chan *c);

// Close the given channel.
// This causes any threads sending or recieving to continue operation.
void chan_close(Chan *c);

// Recieve from channel.
// Blocking recieve a value from the channel.
// If the channel is closed, returns a null pointer.
void *chan_recv(Chan *c);

// Send a value over a channel.
void chan_send(Chan *c, void *v);

// Take an array of SeletOp of size n
// perform one of the actions.
// Returns the index of the action taken or -1 if no action was taken.
// If shouldblock is true, waits for an operation to complete.
int chan_select(SelectOp [], int n, int shouldblock);

