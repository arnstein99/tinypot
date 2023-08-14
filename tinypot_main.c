static const char* version = "1.9";

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tinypot_process.h"

int main(int argc, char* argv[])
{
    int socketFD;
    int port_num;
    int index;
    char* address_arg;
    int con_num;
    int iarg;
    int* port_array;
    struct pollfd* pds;
    int num_ports;

    printf("Program tinypot version %s\n", version);

    address_arg = "";
    switch(argc)
    {
    case 1:
    case 2:
        fprintf(stderr, "Usage: tinypot [address|-] portnum portnum ...\n");
        exit(EXIT_FAILURE);
        break;
    default:
        address_arg = argv[1];
        break;
    }

    if (strcmp(address_arg, "-") == 0)
    {
        address_arg = "*";
    }
    num_ports = argc - 2;
    if ((port_array = (int*)malloc(num_ports * sizeof(int))) == NULL)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    if ((pds = (struct pollfd*)malloc(num_ports * sizeof(struct pollfd))) ==
        NULL)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        perror("signal failed");
        exit(EXIT_FAILURE);
    }
    if (signal(SIGTRAP, SIG_IGN) == SIG_ERR)
    {
        perror("signal failed");
        exit(EXIT_FAILURE);
    }

    memset(pds, 0, num_ports * sizeof(struct pollfd));
    for (iarg = 2 ; iarg < argc ; ++iarg)
    {
        char* port_arg = argv[iarg];
        struct sockaddr_in sa;
        if (sscanf(port_arg, "%d", &port_num) != 1)
        {
            fprintf(stderr, "Illegal numeric expression \"%s\"\n", port_arg);
            exit(EXIT_FAILURE);
        }
        socketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socketFD == -1)
        {
            perror("cannot create socket");
            exit(EXIT_FAILURE);
        }
        int reuse = 1;
        if (setsockopt(
            socketFD, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        {
                perror("cannot set SO_REUSEADDR");
                exit(EXIT_FAILURE);
        }

        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons((uint16_t)port_num);

        if (strcmp(address_arg, "*") == 0)
        {
                sa.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else
        {
            if ((sa.sin_addr.s_addr = inet_addr(address_arg)) == INADDR_NONE)
            {
                fprintf(stderr,
                    "Cannot use \"%s\" with -listen.\n", address_arg);
                fprintf(stderr, "Please use numbers, e.g. nn.nn.nn.nn .\n");
                exit(EXIT_FAILURE);
            }
        }

        if (bind(socketFD, (struct sockaddr *)(&sa), (socklen_t)sizeof(sa))
            == -1)
        {
            fprintf(stderr, "On port %d, ", port_num);
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(socketFD, 10) == -1)
        {
            perror("listen failed");
            exit(EXIT_FAILURE);
        }
        port_array[iarg-2] = port_num;
        pds[iarg-2].fd     = socketFD;
        pds[iarg-2].events = POLLIN;
    }

    printf("%s Listening on address %s, %d TCP/IP ports:\n",
        my_time(), address_arg, num_ports);
    printf("    ");
    for (index = 0 ; index < num_ports ; ++index)
        printf(" %d", port_array[index]);
    printf("\n");
    fflush(stdout);

    con_num = 0;
    while (1)
    {
        int status = poll(pds, num_ports, -1);
        if (status < 0)
        {
            perror("poll failed");
            exit(EXIT_FAILURE);
        }
        if (status == 0)
        {
            fprintf(stderr, "Unexpected 0 return from poll()\n");
            exit(EXIT_FAILURE);
        }
        for (index = 0 ; index < num_ports ; ++index)
        {
            if (pds[index].revents == 0) continue;
            if (pds[index].revents != POLLIN)
            {
                fprintf(stderr, "Unexpected revents value %d (1)\n",
                    pds[index].revents);
                exit(EXIT_FAILURE);
            }
            if (process_connection(++con_num, port_array[index],
                pds[index].fd) != 0) exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}
