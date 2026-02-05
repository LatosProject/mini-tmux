#ifndef UTIL_H
#define UTIL_H

#include <sys/socket.h>

const char *getshell(void);
int checkshell(const char *shell);

struct environ_entry {
	char		*name;
	char		*value;
	int		 flags;
};

int client_check_nested(void);
int send_fd(int sock, int fd);
int recv_fd(int sock);

#endif /* UTIL_H */
