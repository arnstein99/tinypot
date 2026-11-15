#ifndef TINYPOT_COUNTER_H
#define TINYPOT_COUNTER_H

/*
 * A small counting semaphore for threads in one process.
 *
 * The control structure is opaque to callers. Initialize it exactly once
 * before use, and destroy it only after all users and waiters have stopped.
 * Functions return 0 on success and 1 on error, setting errno to the error.
 */
struct counter_control_impl;
typedef struct {
    struct counter_control_impl *implementation;
} counter_control;

int counter_init(unsigned int count, counter_control *control);
int counter_request(counter_control *control);
int counter_release(counter_control *control);
int counter_query(counter_control *control, int *count);
int counter_destroy(counter_control *control);

/* Convenience layer */
counter_control* ccounter_init(unsigned int count);
void ccounter_request(counter_control* control);
void ccounter_release(counter_control* control);
int ccounter_query(counter_control* control);
void ccounter_destroy(counter_control* control);

#endif
