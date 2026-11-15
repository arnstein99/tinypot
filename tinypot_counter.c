#define _POSIX_C_SOURCE 200809L

#include "tinypot_counter.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct counter_control_impl {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    unsigned int count;
};

static int pthread_error(int error)
{
    if (error != 0) {
        errno = error;
        return 1;
    }
    return 0;
}

int counter_init(unsigned int count, counter_control *control)
{
    int error;

    if (control == NULL) {
        errno = EINVAL;
        return 1;
    }

    struct counter_control_impl *state = malloc(sizeof(*state));
    if (state == NULL) {
        if (errno == 0) errno = ENOMEM;
        return 1;
    }

    error = pthread_mutex_init(&state->mutex, NULL);
    if (error != 0) {
        free(state);
        return pthread_error(error);
    }

    error = pthread_cond_init(&state->condition, NULL);
    if (error != 0) {
        (void)pthread_mutex_destroy(&state->mutex);
        free(state);
        return pthread_error(error);
    }

    state->count = count;
    control->implementation = state;
    return 0;
}

int counter_request(counter_control *control)
{
    if (control == NULL || control->implementation == NULL) {
        errno = EINVAL;
        return 1;
    }

    struct counter_control_impl *state = control->implementation;
    int error = pthread_mutex_lock(&state->mutex);
    if (pthread_error(error)) return 1;

    while (state->count == 0) {
        error = pthread_cond_wait(&state->condition, &state->mutex);
        if (error != 0) {
            (void)pthread_mutex_unlock(&state->mutex);
            return pthread_error(error);
        }
    }

    --state->count;
    error = pthread_mutex_unlock(&state->mutex);
    return pthread_error(error);
}

int counter_release(counter_control *control)
{
    if (control == NULL || control->implementation == NULL) {
        errno = EINVAL;
        return 1;
    }

    struct counter_control_impl *state = control->implementation;
    int error = pthread_mutex_lock(&state->mutex);
    if (pthread_error(error)) return 1;

    if (state->count == UINT_MAX) {
        (void)pthread_mutex_unlock(&state->mutex);
        errno = EOVERFLOW;
        return 1;
    }

    ++state->count;
    error = pthread_cond_signal(&state->condition);
    int unlock_error = pthread_mutex_unlock(&state->mutex);
    if (error != 0) return pthread_error(error);
    return pthread_error(unlock_error);
}

int counter_query(counter_control *control, int *count)
{
    if (control == NULL || control->implementation == NULL || count == NULL) {
        errno = EINVAL;
        return 1;
    }

    struct counter_control_impl *state = control->implementation;
    int error = pthread_mutex_lock(&state->mutex);
    if (pthread_error(error)) return 1;

    if (state->count > INT_MAX) {
        (void)pthread_mutex_unlock(&state->mutex);
        errno = EOVERFLOW;
        return 1;
    }

    *count = (int)state->count;
    error = pthread_mutex_unlock(&state->mutex);
    return pthread_error(error);
}

int counter_destroy(counter_control *control)
{
    if (control == NULL || control->implementation == NULL) {
        errno = EINVAL;
        return 1;
    }

    struct counter_control_impl *state = control->implementation;
    int error = pthread_cond_destroy(&state->condition);
    if (error != 0) return pthread_error(error);

    error = pthread_mutex_destroy(&state->mutex);
    if (error != 0) return pthread_error(error);

    free(state);
    control->implementation = NULL;
    return 0;
}

counter_control* ccounter_init(unsigned int count)
{
    counter_control* control = NULL;
    if (counter_init(count, control) != 0)
    {
        perror("counter_init");
        exit(1);
    }
    return control;
}

void ccounter_request(counter_control* control)
{
    if (counter_request(control) != 0)
    {
        perror("counter_init");
        exit(1);
    }
}

void ccounter_release(counter_control* control)
{
    if (counter_release(control) != 0)
    {
        perror("counter_release");
        exit(1);
    }
}

int ccounter_query(counter_control* control)
{
    int count;
    if (counter_query(control, &count) != 0)
    {
        perror("counter_query");
        exit(1);
    }
    return count;
}

void ccounter_destroy(counter_control* control)
{
    if (counter_destroy(control) != 0)
    {
        perror("counter_destroy");
        exit(1);
    }
}
