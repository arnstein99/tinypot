#ifndef _TINYPOT_PROCESS_H_
#define _TINYPOT_PROCESS_H_

#include <stdbool.h>
#include "tinypot_counter.h"

void process_init(void);
void process_destroy(void);
int process_connection (
    bool do_shtup, bool do_yawn, int con_num, int port_num, int socketFD);
char* my_time (void);

#endif /* _TINYPOT_PROCESS_H_ */
