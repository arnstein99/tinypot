#ifndef _TINYPOT_PROCESS_H_
#define _TINYPOT_PROCESS_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void process_connection (int con_num, int port_num, int socketFD);
char* my_time (void);

#endif /* _TINYPOT_PROCESS_H_ */
