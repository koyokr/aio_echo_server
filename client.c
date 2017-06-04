#include "client.h"
#include "aiocb.h"
#include "error.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

int g_fd;

#define M_NEW "\x1b[0;32m[*] new client\x1b[0m"
#define M_DEL "\x1b[0;31m[*] delete client\x1b[0m"

static int new_client(int fd)
{
	struct sockaddr_in sa;
	socklen_t slen;

	int cfd = accept(fd, (struct sockaddr *)&sa, &slen);
	if (cfd == -1)
		unix_error("accept");
	if (write(STDOUT_FILENO, M_NEW, sizeof(M_NEW)) == -1)
		unix_error("write");

	return cfd;
}

static void delete_client(int fd)
{
	close(fd);
	if (write(STDOUT_FILENO, M_DEL, sizeof(M_DEL)) == -1)
		unix_error("write");
}

void aio_completion_handler(sigval_t sigval)
{
	struct aiocb *cbp = sigval.sival_ptr;
	int fd = cbp->aio_fildes;

	if (fd == g_fd) { /* new client */
		int cfd = new_client(fd);
		struct aiocb *ccbp = new_aiocb(cfd);

		if (aio_read(ccbp) == -1)
			unix_error("aio_read");

		goto next;
	}

	char *buf = (void *)cbp->aio_buf;
	int buflen = aio_return(cbp);

	if (fd == STDIN_FILENO) { /* stdin */
		buf[buflen] = '\0';

		if (strcmp(buf, "exit\n") == 0)
			exit(0);

		goto next;
	}
	else if (buflen > 0) { /* echo */
		buf[buflen] = '\0';

		if (write(fd, buf, buflen) == -1)
			unix_error("write");

		if (write(STDOUT_FILENO, buf, buflen) == -1)
			unix_error("write");

		goto next;
	}
	else { /* delete client */
		delete_client(fd);
		delete_aiocb(cbp);

		goto end;
	}

end:
	return;
next:
	if (aio_read(cbp) == -1)
		unix_error("aio_read");
}
