#include "aiocb.h"
#include "client.h"
#include <stdlib.h>

#define BUFLEN 1024

struct aiocb *new_aiocb(int fd)
{
	struct aiocb *cbp = malloc(sizeof(struct aiocb));

	cbp->aio_fildes = fd;
	cbp->aio_buf    = malloc(BUFLEN);
	cbp->aio_nbytes = BUFLEN;
	cbp->aio_offset = 0;
	cbp->aio_lio_opcode = LIO_READ;
	cbp->aio_sigevent.sigev_notify            = SIGEV_THREAD;
	cbp->aio_sigevent.sigev_notify_function   = aio_completion_handler;
	cbp->aio_sigevent.sigev_notify_attributes = NULL;
	cbp->aio_sigevent.sigev_value.sival_ptr   = cbp;

	return cbp;
}

void delete_aiocb(struct aiocb *cbp)
{
	free((void *)cbp->aio_buf);
	free(cbp);
}
