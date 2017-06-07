#include "client.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

static int open_server(int domain, int port)
{
	int y = 1;
	struct sockaddr_in sa = {
		.sin_family      = domain,
		.sin_port        = htons(port),
		.sin_addr.s_addr = htonl(INADDR_ANY)
	};
	int fd = socket(domain, SOCK_STREAM, 0);

	if (fd == -1)
		unix_error("socket");
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(y)) == -1)
		unix_error("setsockopt");
	if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
		unix_error("bind");
	if (listen(fd, 5) == -1)
		unix_error("listen");

	return fd;
}

static void help(char const *path)
{
	fprintf(stderr, "usage: %s <port> [-eb]\n", path);
	exit(1);
}

int main(int argc, char *argv[])
{
	int fd;
	struct aiocb *cblist[2];

	if (argc == 2)
		g_eb = 0;
	else if (argc == 3 && strcmp(argv[2], "-eb") == 0)
		g_eb = 1;
	else
		help(argv[0]);

	fd = open_server(AF_INET, atoi(argv[1]));
	cblist[0] = new_aiocb(STDIN_FILENO);
	cblist[1] = new_aiocb(fd);

	g_fd = fd;

	if (lio_listio(LIO_NOWAIT, cblist, 2, NULL) == -1)
		unix_error("lio_listio");

	pause();
	return 0;
}
