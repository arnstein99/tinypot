#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include "tinypot_process.h"

struct Arg
{
    int con_num;
    int port_num;
    int socketFD;
    int connectFD;
    struct sockaddr_in addr;
};

static void* worker (void* arg);

void process_connection (int con_num, int port_num, int socketFD)
{
    int connectFD;
    pthread_t thread_handle;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    struct Arg* parg;

    if ((parg = (struct Arg*)malloc (sizeof (struct Arg))) == NULL)
    {
        perror ("malloc failed");
	return;
    }

    connectFD = accept (socketFD, (struct sockaddr*)(&addr), &addrlen);
    if (0 > connectFD)
    {
	perror ("accept failed");
	free ((void*)parg);
	return;
    }

    parg->con_num = con_num;
    parg->port_num = port_num;
    parg->socketFD = socketFD;
    parg->connectFD = connectFD;
    parg->addr = addr;

    if (pthread_create (&thread_handle, NULL, worker, (void*)parg) != 0)
    {
        perror ("pthread_create failed");
	free ((void*)parg);
	return;
    }
}

static void* worker (void* arg)
{
    char chr;
    FILE* writeFD;
    struct sockaddr_in local_sa;
    socklen_t local_length = sizeof (local_sa);
    struct Arg* parg = (struct Arg*)arg;
    int printing = 0;
    int iline = 0;

    if (getsockname (
	parg->connectFD, (struct sockaddr*)(&local_sa), &local_length) == -1)
    {
	perror ("getsockname failed");
	close (parg->connectFD);
	free ((void*)parg);
	pthread_exit (NULL);
    }

    if ((writeFD = fdopen (parg->connectFD, "w")) == NULL)
    {
        perror ("fdopen failed");
	close (parg->connectFD);
	free ((void*)parg);
	pthread_exit (NULL);
    }

    fprintf (writeFD, "login: ");
    fflush (writeFD);
    printf ("%s open connection %s -> ",
        my_time(), inet_ntoa (parg->addr.sin_addr));
    printf ("%s:%d    # %d\n",
	inet_ntoa (local_sa.sin_addr), parg->port_num,
	parg->con_num);
    fflush (stdout);
    while (read (parg->connectFD, &chr, 1) != 0)
    {
    	if (!printing)
	{
	    printf ("%s # %d: ", my_time(), parg->con_num);
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
    printf ("%s close connection # %d\n", my_time(), parg->con_num);
    fflush (stdout);

    if (shutdown (parg->connectFD, SHUT_RDWR) == -1)
    {
        perror ("shutdown failed");
	fclose (writeFD);
        close (parg->connectFD);
	free ((void*)parg);
	pthread_exit (NULL);
    }
    fclose (writeFD);
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
	fflush (stderr);
	pthread_exit (NULL);
    }
    tm = *localtime (&tt);
    sprintf (buf, "%04d/%02d/%02d %02d:%02d:%02d",
        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
	tm.tm_hour, tm.tm_min, tm.tm_sec);
    return &buf[0];
}
