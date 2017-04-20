/* gcc -o aio aio.c -lrt */
#include <aio.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void aio_completion_handler(sigval_t sigval);

void set_aiocb(struct aiocb *cbp, int fd) {
    cbp->aio_fildes     = fd;
    cbp->aio_lio_opcode = LIO_READ; /* for lio_listio */
    cbp->aio_buf        = malloc(BUFSIZ);
    cbp->aio_nbytes     = BUFSIZ;
    cbp->aio_offset     = 0;
    cbp->aio_sigevent.sigev_notify            = SIGEV_THREAD;
    cbp->aio_sigevent.sigev_notify_function   = aio_completion_handler;
    cbp->aio_sigevent.sigev_notify_attributes = NULL;
    cbp->aio_sigevent.sigev_value.sival_ptr   = cbp;
}

void unix_error(const char *s) {
    perror(s);
    exit(EXIT_FAILURE);
}

int g_fd;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s <port>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in ssin = {
        .sin_family      = AF_INET,
        .sin_port        = htons(atoi(argv[1])),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    /* socket, bind, listen */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        unix_error("socket");
    if (bind(fd, (struct sockaddr *)&ssin, sizeof(struct sockaddr)) == -1)
        unix_error("bind");
    if (listen(fd, 5) == -1)
        unix_error("listen");

    /* set */
    struct aiocb cb[2] = {};
    struct aiocb *cblist[2] = { &cb[0], &cb[1] };

    set_aiocb(cblist[0], STDIN_FILENO);
    set_aiocb(cblist[1], fd);

    g_fd = fd;

    /* lio_listio */
    if (lio_listio(LIO_NOWAIT, cblist, 2, NULL) == -1)
        unix_error("lio_listio");

    pause();
}

void aio_completion_handler(sigval_t sigval) {
    struct aiocb *cbp = sigval.sival_ptr;
    if (aio_error(cbp) == -1)
        unix_error("aio_handler");

    char *buf = (char *)cbp->aio_buf;
    int buflen = aio_return(cbp);

    if (cbp->aio_fildes == STDIN_FILENO) {
        /* exit */
        buf[buflen] = '\0';
        if (!strcmp(buf, "exit\n"))
            exit(EXIT_SUCCESS);

        /* aio_read */
        if (aio_read(cbp) == -1)
            unix_error("aio_read");
    }
    else if (cbp->aio_fildes == g_fd) {
        /* accept */
        struct sockaddr_in csin;
        socklen_t socklen = sizeof(struct sockaddr);

        int cfd = accept(g_fd, (struct sockaddr *)&csin, &socklen);
        if (cfd == -1)
            unix_error("accept");
        printf("\x1b[0;32m[*] aceept\x1b[0m\n");

        /* new client */
        struct aiocb *ccbp = malloc(sizeof(struct aiocb));
        set_aiocb(ccbp, cfd);

        /* aio_read */
        if (aio_read(cbp)  == -1 ||
            aio_read(ccbp) == -1)
            unix_error("aio_read");
    }
    else {
        int cfd = cbp->aio_fildes;
        if (buflen <= 0) {
            /* close */
            close(cfd);
            printf("\x1b[0;31m[*] close\x1b[0m\n");

            /* delete client */
            free(buf);
            free(cbp);
        }
        else {
            /* write */
            buf[buflen] = '\0';
            if (write(cfd, buf, buflen) != buflen)
                unix_error("write");
            printf("%s", buf);

            /* aio_read */
            if (aio_read(cbp) == -1)
                unix_error("aio_read");
        }
    }
}
