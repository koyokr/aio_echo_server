#include <aio.h>

extern int g_fd;

extern struct aiocb *new_aiocb(int fd);
extern void delete_aiocb(struct aiocb *cbp);
