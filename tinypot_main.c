#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tinypot_process.h"

#define INVALID_SOCKET (-1)
#define MAX(a,b) \
    ((a) > (b) ? (a) : (b))

typedef struct
{
    int port_num;
    int fd;
}
Record;

int main (int argc, char* argv[])
{
    fd_set master_fds;
    int socketFD;
    int port_num;
    char* address_arg;
    int con_num;
    int iarg;
    Record* record;
    int num_ports;
    int maxfd = -1;

    switch (argc)
    {
    case 1:
    case 2:
        fprintf (stderr, "Usage: tinypot [address|-] portnum portnum ...\n");
	exit (1);
        break;
    default:
        address_arg = argv[1];
        break;
    }

    if (strcmp (address_arg, "-") == 0)
    {
        address_arg = "*";
    }
    num_ports = argc - 2;
    if ((record = (Record*)malloc (num_ports * sizeof (Record))) == NULL)
    {
        perror ("malloc");
	exit (EXIT_FAILURE);
    }
    FD_ZERO (&master_fds);
    for (iarg = 2 ; iarg < argc ; ++iarg)
    {
	char* port_arg = argv[iarg];
	struct sockaddr_in sa;
	if (sscanf (port_arg, "%d", &port_num) != 1)
	{
	    fprintf (stderr, "Illegal numeric expression \"%s\"\n", port_arg);
	    exit (1);
	}
	socketFD = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socketFD == -1)
	{
	    perror ("cannot create socket");
	    exit (EXIT_FAILURE);
	}

	memset (&sa, 0, sizeof (sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons (port_num);

	if (strcmp (address_arg, "*") == 0)
	    sa.sin_addr.s_addr = htonl (INADDR_ANY);
	else
	    sa.sin_addr.s_addr = inet_addr (address_arg);

	if (bind (socketFD, (struct sockaddr *)(&sa), sizeof (sa)) == -1)
	{
	    perror ("bind failed");
	    exit (EXIT_FAILURE);
	}

	if (listen (socketFD, 10) == -1)
	{
	    perror ("listen failed");
	    exit (EXIT_FAILURE);
	}
	FD_SET (socketFD, &master_fds);
	maxfd = MAX (maxfd, socketFD);
	record[iarg-2].port_num = port_num;
	record[iarg-2].fd = socketFD;
    }

    printf ("%s Listening on %d TCP/IP ports: address %s\n",
        my_time(), num_ports, address_arg);

    for (con_num = 1 ; ; ++con_num)
    {
	int index;
        int connectFD;
	int status;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	fd_set read_fds = master_fds;

	status = select (maxfd+1, &read_fds, NULL, NULL, NULL);
	if (status < 0)
	{
	    perror ("select");
	    continue;
	}
	socketFD = INVALID_SOCKET;
	for (index = 0 ; index < num_ports ; ++index)
	{
	    if (FD_ISSET (record[index].fd, &read_fds))
	    {
	        socketFD = record[index].fd;
		port_num = record[index].port_num;
		break;
	    }
	}
	if (socketFD == INVALID_SOCKET)
	{
	    fprintf (stderr, "%s: spurious connection\n", my_time());
	    continue;
	}

        connectFD = accept (socketFD, (struct sockaddr*)(&addr), &addrlen);
        if (0 > connectFD)
        {
            perror ("accept failed");
            exit (EXIT_FAILURE);
        }

	process_connection (con_num, port_num, connectFD, &(addr.sin_addr));
    }

    return EXIT_SUCCESS;
}
