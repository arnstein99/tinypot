#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "tinypot_counter.h"

static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static int process_counter = 0;

void counter_increment(int delta)
{
    pthread_mutex_lock(&counter_mutex);
    process_counter += delta;
    if (process_counter < 0)
    {
        fprintf(stderr, "Programming error in process counter\n");
        pthread_mutex_unlock(&counter_mutex);
        exit(1);
    }
    pthread_mutex_unlock(&counter_mutex);
}

int counter_get()
{
    int retval;
    pthread_mutex_lock(&counter_mutex);
    retval = process_counter;
    pthread_mutex_unlock(&counter_mutex);
    return retval;
}
