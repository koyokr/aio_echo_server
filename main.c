#include <aio.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void aio_completion_handler(sigval_t sigval);

struct aiocb *new_aiocb(int fd)
{
    struct aiocb *cbp = malloc(sizeof(struct aiocb));

    cbp->aio_fildes = fd;
    cbp->aio_buf    = malloc(BUFSIZ);
    cbp->aio_nbytes = BUFSIZ;
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

void unix_error(const char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}

int g_fd;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        puts("usage: aio_echo_server <port>");
        return 1;
    }

    struct sockaddr_in sa = {
        .sin_family      = AF_INET,
        .sin_port        = htons(atoi(argv[1])),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    /* socket, bind, listen */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        unix_error("socket");
    if (bind(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr)) == -1)
        unix_error("bind");
    if (listen(fd, 5) == -1)
        unix_error("listen");

    /* set */
    struct aiocb *cblist[2] = {
        new_aiocb(STDIN_FILENO),
        new_aiocb(fd)
    };

    g_fd = fd;

    /* lio_listio */
    if (lio_listio(LIO_NOWAIT, cblist, 2, NULL) == -1)
        unix_error("lio_listio");

    pause();
}

void aio_completion_handler(sigval_t sigval)
{
    struct aiocb *cbp = sigval.sival_ptr;
    int fd = cbp->aio_fildes;
    char *buf = (void *)cbp->aio_buf;
    int buflen = aio_return(cbp);

    if (fd == STDIN_FILENO) { /* stdin */
        buf[buflen] = '\0';
        if (!strcmp(buf, "exit\n"))
            exit(EXIT_SUCCESS);
    }
    else if (fd == g_fd) { /* new client */
        struct sockaddr_in sa;
        socklen_t slen;

        int cfd = accept(fd, (struct sockaddr *)&sa, &slen);
        if (cfd == -1)
            unix_error("accept");
        puts("\x1b[0;32m[*] aceept\x1b[0m");

        struct aiocb *ccbp = new_aiocb(cfd);
        if (aio_read(ccbp) == -1)
            unix_error("aio_read");
    }
    else {
        if (buflen <= 0) { /* delete client */
            close(fd);
            puts("\x1b[0;31m[*] close\x1b[0m");

            delete_aiocb(cbp);
            return;
        }
        else { /* echo */
            buf[buflen] = '\0';
            if (write(fd, buf, buflen) != buflen)
                unix_error("write");
            puts(buf);
        }
    }

    /* aio_read */
    if (aio_read(cbp) == -1)
        unix_error("aio_read");
}
