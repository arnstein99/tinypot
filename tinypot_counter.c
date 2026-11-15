#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include "tinypot_counter.h"

static sem_t sem;
static unsigned int max_count;

void counter_init(unsigned int count)
{
    max_count = count;
    if (sem_init(&sem, 0, count) != 0)
    {
        perror("sem_init");
        exit(1);
    }
}

void counter_request(void)
{
    if (sem_wait(&sem) != 0)
    {
        perror("sem_wait");
        exit(1);
    }
}

void counter_release(void)
{
    if (sem_post(&sem) != 0)
    {
        perror("sem_post");
        exit(1);
    }
}

int counter_query(void)
{
    int retval;
    if(sem_getvalue(&sem, &retval) != 0)
    {
        perror("sem_getvalue");
        exit(1);
    }
    if (retval < 0) retval = 0;
    retval = max_count - retval;
    return retval;
}
