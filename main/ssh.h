#ifndef SSH_H
#define SSH_H

int ssh_open(void);
int ssh_send_command(const char *command, int test);
void ssh_close2(void);

#endif

