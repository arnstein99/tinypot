#ifndef _TINYPOT_PROCESS_H_
#define _TINYPOT_PROCESS_H_

#include <stdbool.h>
#include "tinypot_counter.h"

extern counter_control* ccm;

int process_connection (
    bool do_shtup, int con_num, int port_num, int socketFD);
char* my_time (void);

#endif /* _TINYPOT_PROCESS_H_ */
