#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tinypot_process.h"

int main (int argc, char* argv[])
{
    struct sockaddr_in sa;
    int SocketFD;
    int port_num;
    char* address_arg;
    char* port_arg;
    int con_num;

    switch (argc)
    {
    case 2:
        address_arg = "-";
	port_arg = argv[1];
        break;
    case 3:
        address_arg = argv[1];
	port_arg = argv[2];
        break;
    default:
        fprintf (stderr, "Usage: tinypot [address] portnum\n");
	exit (1);
        break;
    }

    if (sscanf (port_arg, "%d", &port_num) != 1)
    {
        fprintf (stderr, "Illegal numeric expression \"%s\"\n", port_arg);
	exit (1);
    }

    SocketFD = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (SocketFD == -1)
    {
        perror ("cannot create socket");
        exit (EXIT_FAILURE);
    }

    memset (&sa, 0, sizeof sa);

    sa.sin_family = AF_INET;
    sa.sin_port = htons (port_num);
    if (strcmp (address_arg, "-") == 0)
    {
	sa.sin_addr.s_addr = htonl (INADDR_ANY);
	address_arg = "*";
    }
    else
	sa.sin_addr.s_addr = inet_addr (address_arg);

    if (bind (SocketFD, (struct sockaddr *) &sa, sizeof sa) == -1)
    {
        perror ("bind failed");
        close (SocketFD);
        exit (EXIT_FAILURE);
    }

    if (listen (SocketFD, 10) == -1)
    {
        perror ("listen failed");
        close (SocketFD);
        exit (EXIT_FAILURE);
    }
    printf ("%s Listening on TCP/IP port:socket %s:%d\n",
        my_time(), address_arg, port_num);
    fflush (stdout);

    for (con_num = 1 ; ; ++con_num)
    {
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
        int ConnectFD = accept (SocketFD, (struct sockaddr*)(&addr), &addrlen);

        if (0 > ConnectFD)
        {
            perror ("accept failed");
            close (SocketFD);
            exit (EXIT_FAILURE);
        }

	process_connection (con_num, ConnectFD, &(addr.sin_addr));
    }

    close (SocketFD);
    return EXIT_SUCCESS;
}
