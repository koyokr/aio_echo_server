#include "client.h"
#include "error.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFLEN 1024
#define M_NEW "\x1b[0;32m[*] new client\x1b[0m\n"
#define M_DEL "\x1b[0;31m[*] delete client\x1b[0m\n"

int g_fd;
int g_eb;

static atomic_int g_fdlist[1024] = {0};
static atomic_int g_fdlist_index = -1;

static void aio_completion_handler(sigval_t sigval);

struct aiocb *new_aiocb(int fd)
{
	struct aiocb *cbp = malloc(sizeof(struct aiocb));

	cbp->aio_fildes  = fd;
	cbp->aio_offset  = 0;
	cbp->aio_buf     = malloc(BUFLEN);
	cbp->aio_nbytes  = BUFLEN;
	cbp->aio_reqprio = 0;
	cbp->aio_sigevent.sigev_notify            = SIGEV_THREAD;
	cbp->aio_sigevent.sigev_signo             = 0;
	cbp->aio_sigevent.sigev_value.sival_ptr   = cbp;
	cbp->aio_sigevent.sigev_notify_function   = aio_completion_handler;
	cbp->aio_sigevent.sigev_notify_attributes = NULL;
	cbp->aio_lio_opcode = LIO_READ;

	return cbp;
}

void delete_aiocb(struct aiocb *cbp)
{
	free((void *)cbp->aio_buf);
	free(cbp);
}

static ssize_t write_wrap(int fd, char const *buf, int buflen)
{
	ssize_t len;

	len = write(fd, buf, buflen);
	if (len == -1)
		unix_error("write");

	return len;
}

static int new_client(int fd)
{
	struct sockaddr_in sa;
	socklen_t slen;
	int cfd;

	cfd = accept(fd, (struct sockaddr *)&sa, &slen);
	if (cfd == -1)
		unix_error("accept");

	write_wrap(STDOUT_FILENO, M_NEW, sizeof(M_NEW));

	return cfd;
}

static void delete_client(int fd)
{
	close(fd);
	write_wrap(STDOUT_FILENO, M_DEL, sizeof(M_DEL));
}

static void write_eb(char const *buf, int buflen)
{
	int i;

	for (i = g_fdlist_index; i > -1; i--)
		write_wrap(g_fdlist[i], buf, buflen);
}

static void aio_completion_handler(sigval_t sigval)
{
	int old_errno = errno;
	struct aiocb *cbp = sigval.sival_ptr;
	int fd = cbp->aio_fildes;

	if (fd == g_fd) {
		// new client
		int cfd = new_client(fd);
		struct aiocb *ccbp = new_aiocb(cfd);

		if (g_eb)
			g_fdlist[++g_fdlist_index] = cfd;

		if (aio_read(ccbp) == -1)
			unix_error("aio_read");

		goto next;
	}

	char *buf = (void *)cbp->aio_buf;
	int buflen = aio_return(cbp);

	if (fd == STDIN_FILENO) {
		// stdin
		if (buflen == -1)
			unix_error("aio_return");

		if (buflen == 0)
			exit(0);

		goto next;
	}

	if (buflen > 0) {
		// echo
		buf[buflen] = '\0';

		if (g_eb)
			write_eb(buf, buflen);
		else
			write_wrap(fd, buf, buflen);

		write_wrap(STDOUT_FILENO, buf, buflen);

		goto next;
	}
	else {
		// delete client
		if (g_eb)
			g_fdlist_index--;

		delete_client(fd);
		delete_aiocb(cbp);

		goto end;
	}

next:
	if (aio_read(cbp) == -1)
		unix_error("aio_read");
end:
	errno = old_errno;
}
