#ifndef _TINYPOT_COUNTER_H_
#define _TINYPOT_COUNTER_H_

void counter_init(unsigned int count);
void counter_request(void);
void counter_release(void);
int counter_query(void);

#endif /* _TINYPOT_COUNTER_H_ */
