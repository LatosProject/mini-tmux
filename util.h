#include <sys/socket.h>
const char *getshell();
int checkshell(const char *shell);
struct environ_entry {
	char		*name;
	char		*value;
	int		 flags;
};
int client_check_nested();
int send_fd(int sock, int fd); 
int recv_fd(int sock);         
