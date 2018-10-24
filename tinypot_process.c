#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include "tinypot_process.h"

/* The following constant will be multiplied by a million, so don't let it get
 * much larger than 2000. */
#define MY_MAX 10 /* seconds */

struct Arg
{
    int con_num;
    int port_num;
    int socketFD;
    int connectFD;
    struct sockaddr_in addr;
};

static void* worker (void* arg);
static void timestamp (FILE* fd, int con_num, int colon);
static void my_sleep (void);

int process_connection (int con_num, int port_num, int socketFD)
{
    int optval;
    int connectFD;
    pthread_t thread_handle;
    struct sockaddr_in addr;
    socklen_t addrlen = (socklen_t)sizeof(addr);
    struct Arg* parg;

    optval = 1;
    if (setsockopt (
	socketFD, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval))
	< 0)
    {
	perror("cannot set keepalive");
	exit (EXIT_FAILURE);
    }

    if ((parg = (struct Arg*)malloc (sizeof (struct Arg))) == NULL)
    {
	timestamp (stderr, parg->con_num, 0);
        perror ("malloc failed");
	return 1;
    }

    connectFD = accept (socketFD, (struct sockaddr*)(&addr), &addrlen);
    if (0 > connectFD)
    {
	timestamp (stderr, parg->con_num, 0);
	perror ("accept failed");
	free ((void*)parg);
	if (errno == EMFILE) return 1;
	return 0;
    }

    parg->con_num = con_num;
    parg->port_num = port_num;
    parg->socketFD = socketFD;
    parg->connectFD = connectFD;
    parg->addr = addr;

    if (pthread_create (&thread_handle, NULL, worker, (void*)parg) != 0)
    {
	timestamp (stderr, parg->con_num, 0);
        perror ("pthread_create failed");
	free ((void*)parg);
	return 1;
    }
    if (pthread_detach (thread_handle) != 0)
    {
        perror ("pthread_detach failed");
	free ((void*)parg);
	return 1;
    }
    return 0;
}

static void* worker (void* arg)
{
    char chr;
    int retval;
    FILE* writeFD;
    struct sockaddr_in local_sa;
    socklen_t local_length = (socklen_t)sizeof (local_sa);
    struct Arg* parg = (struct Arg*)arg;
    int printing = 0;    /* 2 if a \n, time stamp, and char(s) have been sent,
                          * 1 if a \n, time stamp have been sent,
			  * 0 if a \n has been sent.
			  * NO OTHER CONDITIONS ALLOWED.
			  */
    int iline = 0;

    if (getsockname (
	parg->connectFD, (struct sockaddr*)(&local_sa), &local_length) == -1)
    {
	timestamp (stderr, parg->con_num, 0);
	perror ("getsockname failed");
	close (parg->connectFD);
	free ((void*)parg);
	pthread_exit (NULL);
    }

    if ((writeFD = fdopen (parg->connectFD, "w")) == NULL)
    {
	timestamp (stderr, parg->con_num, 0);
        perror ("fdopen failed");
	close (parg->connectFD);
	free ((void*)parg);
	pthread_exit (NULL);
    }

    my_sleep();
    fprintf (writeFD, "login: ");
    fflush (writeFD);
    timestamp (stdout, parg->con_num, 0);
    printf ("open connection %s -> ", inet_ntoa (parg->addr.sin_addr));
    printf ("%s:%d\n",
	inet_ntoa (local_sa.sin_addr), parg->port_num);
    fflush (stdout);
    printing = 0;
    while ((retval = read (parg->connectFD, &chr, 1)) > 0)
    {
    	if (printing == 0)
	{
	    timestamp (stdout, parg->con_num, 1);
	    fflush (stdout);
	    printing = 1;
	}
	if (iline > 1)
	    putc (chr, writeFD);
	putchar (chr);
	if (chr == '\n')
	{
	    my_sleep();
	    if (iline == 0)
	    {
	        fprintf (writeFD, "Password:");
	    }
	    /* TODO: delete this clause */
	    else if (iline == 1)
	    {
	        fprintf (writeFD, "$ ");
	    }
	    else
	    {
	        fprintf (writeFD, "$ ");
	    }
	    fflush (writeFD);
	    fflush (stdout);
	    ++iline;
	    printing = 0;
	}
	else
	{
	    printing = 2;
	}
    }
    if (printing == 2)
    {
	printf ("\n");
	timestamp (stdout, parg->con_num, 0);
        printf ("(Missing newline)\n");
	printing = 0;
    }
    if (printing == 0)
    {
	timestamp (stdout, parg->con_num, 0);
	printing = 1;
    }
    fflush (stdout);
    if (retval == -1)
	perror ("close connection");
    else
	fprintf (stderr, "close connection: end of file\n");
    fflush (stderr);
    printing = 0;

    /* This fails so often with "endpoint is not connected" that it is not
     * interesting */
    shutdown (parg->connectFD, SHUT_RDWR);

    fclose (writeFD);
    close (parg->connectFD);
    free ((void*)parg);
    pthread_exit (NULL);
}

static void timestamp (FILE* fd, int con_num, int colon)
{
    fprintf (fd, "%s #%d", my_time(), con_num);
    fprintf (fd, "%s ", (colon ? ":" : " "));
}

char* my_time (void)
{
    time_t tt;
    struct tm tm;
    static __thread char buf[128];
    if ((tt = time (NULL)) == -1)
    {
	perror ("time failed");
	pthread_exit (NULL);
    }
    tm = *localtime (&tt);
    snprintf (buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d:%02d",
        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
	tm.tm_hour, tm.tm_min, tm.tm_sec);
    return &buf[0];
}

static void my_sleep (void)
{
    static const double my_scale = (double)MY_MAX * 1000000 / RAND_MAX;
    unsigned int sleep_time = rand();
    sleep_time = (unsigned int)(my_scale * sleep_time);  /* seconds */
    while (sleep_time >= 500000)
    {
	usleep (500000);
	sleep_time -= 500000;
    }
    usleep (sleep_time);
}
