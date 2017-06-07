#include "client.h"
#include "error.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUF_SIZE 1024

#define FD_MIN (STDERR_FILENO + 1)
#define FD_MAX 256

#define M_NEW "\x1b[0;32m[*] new client\x1b[0m\n"
#define M_DEL "\x1b[0;31m[*] delete client\x1b[0m\n"

int g_fd;
int g_eb;

static atomic_int g_fd_map[FD_MAX] = {0};

static ssize_t write_wrap(int fd, char const *buf, int buflen)
{
	ssize_t len = write(fd, buf, buflen);

	if (len == -1)
		unix_error("write");
	return len;
}

static void write_eb(char const *buf, int buflen)
{
	int fd;

	for (fd = FD_MIN; fd < FD_MAX; fd++)
		if (g_fd_map[fd])
			write_wrap(fd, buf, buflen);
}

static int new_client(int fd)
{
	struct sockaddr_in sa;
	socklen_t slen;
	int cfd = accept(fd, (struct sockaddr *)&sa, &slen);

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

static void aio_completion_handler(sigval_t sigval)
{
	struct aiocb *cbp = sigval.sival_ptr;
	int fd = cbp->aio_fildes;

	if (fd == g_fd) {
		// new client
		int cfd = new_client(fd);
		struct aiocb *ccbp = new_aiocb(cfd);

		if (g_eb)
			g_fd_map[cfd] = 1;

		if (aio_read(ccbp) == -1)
			unix_error("aio_read");
	}
	else {
		int buflen = aio_return(cbp);

		if (fd == STDIN_FILENO) {
			// stdin
			if (buflen == -1)
				unix_error("aio_return");
			else if (buflen == 0)
				exit(0);
		}
		else if (buflen > 0) {
			// echo
			char *buf = (void *)cbp->aio_buf;

			buf[buflen] = '\0';

			if (g_eb)
				write_eb(buf, buflen);
			else
				write_wrap(fd, buf, buflen);
			write_wrap(STDOUT_FILENO, buf, buflen);
		}
		else {
			// delete client
			if (g_eb)
				g_fd_map[fd] = 0;

			delete_client(fd);
			delete_aiocb(cbp);

			// no aio_read
			return;
		}
	}

	// next aio_read
	if (aio_read(cbp) == -1)
		unix_error("aio_read");
}

struct aiocb *new_aiocb(int fd)
{
	struct aiocb *cbp = malloc(sizeof(struct aiocb));

	cbp->aio_fildes  = fd;
	cbp->aio_offset  = 0;
	cbp->aio_buf     = malloc(BUF_SIZE);
	cbp->aio_nbytes  = BUF_SIZE;
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
