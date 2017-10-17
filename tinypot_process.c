#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include "tinypot_process.h"

struct Arg
{
    int con_num;
    int port_num;
    int connectFD;
    struct in_addr in;
};

static void* worker (void* arg);

void process_connection (
    int con_num, int port_num, int connectFD,  const struct in_addr* in)
{
    pthread_t thread_handle;
    struct Arg* parg;

    if ((parg = (void*)malloc (sizeof (struct Arg))) == NULL)
    {
        perror ("malloc failed");
	return;
    }
    parg->con_num = con_num;
    parg->port_num = port_num;
    parg->connectFD = connectFD;
    parg->in = *in;

    if (pthread_create (&thread_handle, NULL, worker, (void*)parg) != 0)
    {
        perror ("pthread_create failed");
        close (connectFD);
	return;
    }
}

static void* worker (void* arg)
{
    char chr;
    FILE* writeFD;
    struct Arg* parg = (struct Arg*)arg;
    int printing = 0;
    int iline = 0;

    if ((writeFD = fdopen (parg->connectFD, "w")) == NULL)
    {
        perror ("fdopen failed");
	free ((void*)parg);
	pthread_exit (NULL);
    }

    fprintf (writeFD, "login: ");
    fflush (writeFD);
    printf ("%s open connection %s -> port %d    #%d\n",
        my_time(), inet_ntoa (parg->in), 
	parg->port_num, parg->con_num);
    fflush (stdout);
    while (read (parg->connectFD, &chr, 1) != 0)
    {
    	if (!printing)
	{
	    printf ("%s #%d: ", my_time(), parg->con_num);
	    fflush (stdout);
	    printing = 1;
	}
	if (iline > 1)
	    putc (chr, writeFD);
	putchar (chr);
	if (chr == '\n')
	{
	    fflush (writeFD);
	    if (iline == 0)
	        fprintf (writeFD, "Password:");
	    else
	        fprintf (writeFD, "$ ");
	    fflush (writeFD);
	    ++iline;
	    printing = 0;
	}
    }
    if (printing)
        printf ("\n(Missing newline)\n");
    printf ("%s close connection #%d\n", my_time(), parg->con_num);
    fflush (stdout);

    if (shutdown (parg->connectFD, SHUT_RDWR) == -1)
    {
        perror ("shutdown failed");
        close (parg->connectFD);
	free ((void*)parg);
	pthread_exit (NULL);
    }
    close (parg->connectFD);
    free ((void*)parg);
    pthread_exit (NULL);
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
    sprintf (buf, "%04d/%02d/%02d %02d:%02d:%02d",
        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
	tm.tm_hour, tm.tm_min, tm.tm_sec);
    return &buf[0];
}
