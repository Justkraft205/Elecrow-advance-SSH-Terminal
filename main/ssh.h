#ifndef SSH_H
#define SSH_H

int ssh_open(void);
int ssh_shell_open(void);
int ssh_shell_send(const char *command);
int ssh_shell_read(char *buffer, size_t size);
int ssh_send_command(const char *command, int test);
void ssh_close2(void);

#endif
