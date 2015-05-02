
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
// #include <semaphore.h>

// Chan represents a CSP channel.
// Full definition is in chan.c
// The internals of the channel are private.

typedef struct {
    pthread_cond_t  cond;
    pthread_mutex_t lock;
    int             done;
    void            *v;
} blocked;

typedef struct {
    int       sz;
    int       n;
    blocked **pblocked;
} blocked_list;

typedef struct {
    pthread_mutex_t lock;

    blocked_list    senders;
    blocked_list    receivers;

    // If !buffsz then unbuffered.
    // buffered fields
    pthread_cond_t  cond;
    int             buffsz;
    int             nbuff;
    int             bstart;
    int             bend;
    void            **vbuff;

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
