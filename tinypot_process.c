#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include "tinypot_counter.h"
#include "tinypot_process.h"
static const unsigned long e9 =  1000000000;
static const unsigned long e5 =  100000;
static const unsigned long e6 =  1000000;

/*
shtup tuning
------
*/

/* Maximum number of bytes to shtup, each "client". */
static const unsigned long shtup_max = e9;
/* Controls breaks in shtup operation */
static const unsigned long shtup_modulus = e5/2;
/* Throttle for shtup, per client, in seconds */
static const double shtup_throttle = 0.25;
/* Timeout for writing, in seconds. */
static const unsigned shtup_max_wait = 30;
/* Max kernel buffer for writing */
static const int shtup_sndbuf = 2048;

/* Delays will be uniformly distributed between 0 and this number of seconds */
static const unsigned  MY_MAX = 14;

/* Seconds to wait before forcing a newline on output */
#define LINE_WAIT 9

struct Arg
{
    int con_num;
    int port_num;
    int socketFD;
    int connectFD;
    struct sockaddr_in addr;
};

static void* login_worker(void* arg);
static void* shtup_worker(void* arg);
static void timestamp(FILE* fd, int con_num, int colon);
static void my_sleep(double nsec);
/* Note that the semantics of last argument are different
 * across the next two functions */
static int timed_read(
    int d, void* buf, size_t nbyte, const struct timeval* deadline);
static int timed_write(
    int d, void* buf, size_t nbyte, int wait_time);
static void subtractfrom(struct timeval* big, const struct timeval* small);

static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

int process_connection(bool do_shtup, int con_num, int port_num, int socketFD)
{
    int retval;
    int optval;
    int connectFD;
    pthread_t thread_handle;
    struct sockaddr_in addr;
    socklen_t addrlen = (socklen_t)sizeof(addr);
    struct Arg* parg;

    optval = 1;
    if (setsockopt(
        socketFD, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval))
        < 0)
    {
        perror("cannot set keepalive");
        exit(EXIT_FAILURE);
    }

    if ((parg = (struct Arg*)malloc(sizeof(struct Arg))) == NULL)
    {
        timestamp(stderr, 0, 0);
        perror("malloc failed");
        return 1;
    }

    connectFD = accept(socketFD,(struct sockaddr*)(&addr), &addrlen);
    if (0 > connectFD)
    {
        timestamp(stderr, parg->con_num, 0);
        perror("accept failed");
        free((void*)parg);
        if(errno == EMFILE) return 1;
        return 0;
    }

    parg->con_num = con_num;
    parg->port_num = port_num;
    parg->socketFD = socketFD;
    parg->connectFD = connectFD;
    parg->addr = addr;

    retval = do_shtup                                                   ?
        pthread_create(&thread_handle, NULL, shtup_worker, (void*)parg) :
        pthread_create(&thread_handle, NULL, login_worker, (void*)parg) ;
    if (retval != 0)
    {
        timestamp(stderr, parg->con_num, 0);
        perror("pthread_create failed");
        free((void*)parg);
        return 1;
    }
    if (pthread_detach(thread_handle) != 0)
    {
        perror("pthread_detach failed");
        free((void*)parg);
        return 1;
    }

    return 0;
}

static void* login_worker(void* arg)
{
    char chr;
    int retval;
    FILE* writeFD;
    struct sockaddr_in local_sa;
    socklen_t local_length = (socklen_t)sizeof(local_sa);
    struct Arg* parg = (struct Arg*)arg;
    int printing = 0;    /* 2 if a \n, time stamp, and char(s) have been sent,
                          * 1 if a \n, time stamp have been sent,
                          * 0 if a \n has been sent.
                          * NO OTHER CONDITIONS ALLOWED.
                          */
    int iline = 0;
    bool finished;

    /* Accounting */
    counter_increment(1);

    /* Lock mutex early to keep connection numbers (con_num) in order. */
    pthread_mutex_lock(&print_mutex);

    if (getsockname(
        parg->connectFD,(struct sockaddr*)(&local_sa), &local_length) == -1)
    {
        timestamp(stderr, parg->con_num, 0);
        perror("getsockname failed");
        close(parg->connectFD);
        free((void*)parg);
        pthread_mutex_unlock(&print_mutex);
        counter_increment(-1);
        pthread_exit(NULL);
    }

    if ((writeFD = fdopen(parg->connectFD, "w")) == NULL)
    {
        timestamp(stderr, parg->con_num, 0);
        perror("fdopen failed");
        close(parg->connectFD);
        free((void*)parg);
        pthread_mutex_unlock(&print_mutex);
        counter_increment(-1);
        pthread_exit(NULL);
    }

    fprintf(writeFD, "login: ");
    fflush(writeFD);
    timestamp(stdout, parg->con_num, 0);
    printf("open connection %s -> ", inet_ntoa(parg->addr.sin_addr));
    printf("%s:%d\n",
        inet_ntoa(local_sa.sin_addr), parg->port_num);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    printing = 0;
    finished = false;
    my_sleep(MY_MAX);
    struct timeval deadline;
    gettimeofday(&deadline, NULL);
    deadline.tv_sec += LINE_WAIT;

    /* Loop over incoming characters */
    while (1)
    {
        retval = timed_read(parg->connectFD, &chr, 1, &deadline);
        switch (retval)
        {
        case 0:
            /* Got no character */
            finished = true;
            break;
        case 1:
            /* Got a character, keep going */
            break;
        case -1:
            /* Read error */
            finished = true;
            break;
        case -2:
            /* timeout */
            gettimeofday(&deadline, NULL);
            deadline.tv_sec += LINE_WAIT;
            switch (printing)
            {
            case 0:
                /* Mutex already unlocked, nothing to do. */
                break;
            case 1:
                /* Mutex has been locked and a timestamp has been printed */
                printf("(tinypot_wait)\n");
                fflush(stdout);
                pthread_mutex_unlock(&print_mutex);
                break;
            case 2:
                /* Mutex has been locked and some characters have been echoed */
                printf("\n");
                timestamp(stdout, parg->con_num, 0);
                printf("(tinypot_flush)\n");
                fflush(stdout);
                pthread_mutex_unlock(&print_mutex);
                fflush(writeFD);
                break;
            default:
                fprintf(stderr, "Programming error, printing.\n");
                fflush(stderr);
                finished = true;
                break;
            }
            printing = 0;
            continue;
            break;
        default:
            fprintf(stderr, "Programming error, read.\n");
            fflush(stderr);
            finished = true;
            break;
        }
        if (finished) break;
        /* Programming note: chr contains a valid character now. */

        if (printing == 0)
        {
            pthread_mutex_lock(&print_mutex);
            timestamp(stdout, parg->con_num, 1);
            fflush(stdout);
            printing = 1;
        }
        if (iline > 1)
            putc(chr, writeFD);
        putchar(chr);
        if (chr == '\n')
        {
            fflush(stdout);
            printing = 0;
            pthread_mutex_unlock(&print_mutex);
            if (iline == 0)
            {
                fprintf(writeFD, "Password: ");
            }
            else
            {
                fprintf(writeFD, "$ ");
            }
            ++iline;
            fflush(writeFD);
            my_sleep(MY_MAX);
            gettimeofday(&deadline, NULL);
            deadline.tv_sec += LINE_WAIT;
        }
        else
        {
            printing = 2;
        }
    } /* Loop over incoming characters */

    if (printing == 2)
    {
        printf("\n");
        timestamp(stdout, parg->con_num, 0);
        printf("(Missing newline)\n");
        fflush(stdout);
        pthread_mutex_unlock(&print_mutex);
        printing = 0;
    }
    if (printing == 0)
    {
        pthread_mutex_lock(&print_mutex);
        timestamp(stdout, parg->con_num, 0);
        printing = 1;
    }
    printf("close connection: ");
    if (retval == -1)
        printf("%s", strerror(errno));
    else
        printf("end of file");
    printf(" (%d)\n", counter_get() - 1);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
    printing = 0;

    /* This fails so often with "endpoint is not connected" that it is not
     * interesting */
    shutdown(parg->connectFD, SHUT_RDWR);
    close(parg->connectFD);
    fclose(writeFD);

    free((void*)parg);
    counter_increment(-1);
    pthread_exit(NULL);
}

static void* shtup_worker(void* arg)
{
    int retval;
    int rand_val;
    unsigned long shtup_count;
    struct sockaddr_in local_sa;
    socklen_t local_length = (socklen_t)sizeof(local_sa);
    struct Arg* parg = (struct Arg*)arg;
    int wait_time = 1000 * shtup_max_wait;

    /* Limit kernel buffering */
    if (
        setsockopt(
            parg->connectFD, SOL_SOCKET, SO_SNDBUF, &shtup_sndbuf,
            sizeof(shtup_sndbuf))
        != 0)
    {
        timestamp(stderr, parg->con_num, 0);
        perror("setsockopt(SO_SNDBUF)");
        close(parg->connectFD);
        free((void*)parg);
        pthread_exit(NULL);
    }

    /* Accounting */
    counter_increment(1);

    /* Lock mutex early to keep connection numbers (con_num) in order. */
    pthread_mutex_lock(&print_mutex);

    if (getsockname(
        parg->connectFD,(struct sockaddr*)(&local_sa), &local_length) == -1)
    {
        timestamp(stderr, parg->con_num, 0);
        perror("getsockname failed");
        close(parg->connectFD);
        free((void*)parg);
        pthread_mutex_unlock(&print_mutex);
        counter_increment(-1);
        pthread_exit(NULL);
    }

    timestamp(stdout, parg->con_num, 0);
    printf("open connection %s -> ", inet_ntoa(parg->addr.sin_addr));
    printf("%s:%d\n",
        inet_ntoa(local_sa.sin_addr), parg->port_num);
    pthread_mutex_unlock(&print_mutex);

    for (shtup_count = 0 ; shtup_count < shtup_max ; )
    {
        if ((shtup_count % shtup_modulus) == 0)
        {
            my_sleep(shtup_throttle * counter_get());
            pthread_mutex_lock(&print_mutex);
            timestamp(stdout, parg->con_num, 0);
            printf("   ...   %lu bytes   ...\n", shtup_count);
            fflush(stdout);
            pthread_mutex_unlock(&print_mutex);
        }

        rand_val = rand();
        retval = timed_write(
            parg->connectFD, &rand_val, sizeof(rand_val), wait_time);
        if (retval <= 0)
        {
            switch (retval)
            {
            case 0:
                /* Put no data */
                break;
            case -1:
                /* Write error */
                break;
            case -2:
                /* timeout */
                break;
            default:
                fprintf(stderr, "Programming error, write.\n");
                fflush(stderr);
                break;
            }
            if (retval != -1)
            {
                pthread_mutex_lock(&print_mutex);
                timestamp(stdout, parg->con_num, 0);
                printf("Break for retval %d\n", retval);
                pthread_mutex_unlock(&print_mutex);
                break;
            }
            /* This client is finished */
            break;
        }
        else
        {
            shtup_count += retval;
        }
    }

    pthread_mutex_lock(&print_mutex);
    timestamp(stdout, parg->con_num, 0);
    printf("close connection: ");
    if (shtup_count >= 1000000)
    {
        /* Units of 100000 */
        shtup_count = (shtup_count + 50000) / 100000;
        unsigned long millions = shtup_count / 10;
        unsigned long tenths   = shtup_count % 10;
        printf("%lu.%lum bytes sent", millions, tenths);
    }
    else if (shtup_count >= 1000)
    {
        shtup_count = (shtup_count + 500) / 1000;
        printf("%luk bytes sent", shtup_count);
    }
    else
    {
        printf("%lu bytes sent", shtup_count);
    }
    printf(" (%d)\n", counter_get() - 1);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);

    /* This fails so often with "endpoint is not connected" that it is not
     * interesting */
    shutdown(parg->connectFD, SHUT_RDWR);
    close(parg->connectFD);
    free((void*)parg);
    counter_increment(-1);
    pthread_exit(NULL);
}

static void timestamp(FILE* fd, int con_num, int colon)
{
    fprintf(fd, "%s #%d", my_time(), con_num);
    fprintf(fd, "%s ", (colon ? ":" : " "));
}

char* my_time(void)
{
    time_t tt;
    struct tm tm;
    static __thread char buf[128];
    if ((tt = time(NULL)) == -1)
    {
        perror("time failed");
        pthread_exit(NULL);
    }
    tm = *localtime(&tt);
    snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d:%02d",
        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);
    return &buf[0];
}

static void my_sleep(double nsec)
{
    double sleep_time = ( (double)rand() / RAND_MAX ) * nsec;
    struct timespec tv, rem;
    tv.tv_sec = sleep_time;
    tv.tv_nsec = e9 * (sleep_time - tv.tv_sec);
    while (nanosleep(&tv, &rem) != 0)
    {
        tv = rem;
    }
}

static int timed_write(
    int d, void* buf, size_t nbyte, int wait_time)
{
    int status;
    int retval;

    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = d;
    pfd.events = POLLOUT;
    status = poll(&pfd, 1, wait_time);
    switch(status)
    {
    case 0:
        /* timeout */
        retval = -2;
        break;
    case 1:
        /* A character can be sent */
        if (!(pfd.revents & POLLOUT))
        {
            fprintf(stderr, "Unexpected revents value %d (2)\n",
                pfd.revents);
            exit(1);
        }
        /* Programming note: POLLHUP and POLLERR are ignored here. */
        retval = write(d, buf, nbyte);
        break;
    default:
        retval = -3;
        break;
    }
    return retval;
}

static int timed_read(
    int d, void* buf, size_t nbyte, const struct timeval* deadline)
{
    int status;
    int retval;

    struct timeval now;
    gettimeofday(&now, NULL);
    struct timeval delta = *deadline;
    subtractfrom(&delta, &now);
    if ((delta.tv_sec == 0) && (delta.tv_usec == 0))
    {
        return -2;
    }
    int timeout = (1000 * delta.tv_sec) + (delta.tv_usec / 1000);
    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = d;
    pfd.events = POLLIN;
    status = poll(&pfd, 1, timeout);
    switch(status)
    {
    case 0:
        /* timeout */
        retval = -2;
        break;
    case 1:
        /* A character should be available */
        if (!(pfd.revents & POLLIN))
        {
            fprintf(stderr, "Unexpected revents value %d (2)\n",
                pfd.revents);
            exit(1);
        }
        /* Programming note: POLLHUP and POLLERR are ignored here. */
        retval = read(d, buf, nbyte);
        break;
    default:
        retval = -3;
        break;
    }
    return retval;
}

static void subtractfrom(struct timeval* big, const struct timeval* small)
{
    static const struct timeval zero = {0, 0};
    if (big->tv_sec < small->tv_sec)
    {
        *big = zero;
        return;
    }
    big->tv_sec -= small->tv_sec;
    if (big->tv_usec < small->tv_usec)
    {
        if (big->tv_sec < 1)
        {
            *big = zero;
            return;
        }
        --big->tv_sec;
        big->tv_usec += e6;
    }
    big->tv_usec -= small->tv_usec;
}
