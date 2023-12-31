/**
 * @file csapp.c
 * @brief Functions for the CS:APP3e book
 */

#include "csapp.h"

#include <errno.h>      /* errno */
#include <netdb.h>      /* freeaddrinfo() */
#include <semaphore.h>  /* sem_t */
#include <signal.h>     /* struct sigaction */
#include <stdarg.h>     /* va_list */
#include <stdbool.h>    /* bool */
#include <stddef.h>     /* ssize_t */
#include <stdint.h>     /* intmax_t */
#include <stdio.h>      /* stderr */
#include <stdlib.h>     /* abort() */
#include <string.h>     /* memset() */
#include <sys/socket.h> /* struct sockaddr */
#include <sys/types.h>  /* struct sockaddr */
#include <unistd.h>     /* STDIN_FILENO */
#include <unistd.h> //j'en avais besoin pour lseek pour revenir en arrière
#include <fcntl.h>

#define TAILLE_BUFFER 256

/* Nombre d'heures de travail sur le TP par membres:
PAPE Gurwan : 15 heures
MICHOT Maxence : 15 heures

Le commentaire qui suit ne concerne pas le code ci-après mais parle un peu de notre ressenti sur le TP
(par ailleurs partagé par d'autres double licences, voire peut-être infos). Tout au long du codage, nous avons 
ressenti de grandes difficultés à savoir ce qu'on nous demandait de faire lors des questions. Pour donner un exemple, il n'y a aucune indication sur 
l'utilisation concrète de csapp.c pour ouvrir une socket et le cours n'expliquait l'ouvertue de socket que de manière purement théorique.
De même pour ouvrir des sockets en UDP pour la partie Dice Roll. L'essentiel de notre temps de travail a donc été
passé soit à tenter de comprendre du code peu digeste et dont nous étions pas capables de comprendre l'utilité soit
à tenter même de recréer des choses dont on ne nous a expliqués que tardivement qu'elles avaient déjà été construites
entièrement et prêtes à utilisation (dans le manuel de getaddrinfo par exemple, ce que l'on ne pouvait pas deviner).
Nous avons donc ressenti un grand décalage entre le cours très théorique (par ailleurs trés intéréssant) et 
les attendus de ce projet, purement pratique, peu voire aucunement guidé, et n'utilisant peu
voire aucune connaissances du cours.
*/


/************************************
 * Wrappers for Unix signal functions
 ***********************************/

/**
 * @brief   Wrapper for the new sigaction interface. Exits on error.
 * @param signum    Signal to set handler for.
 * @param handler   Handler function.
 *
 * @return  Previous disposition of the signal.
 */
handler_t *Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0) {
        perror("Signal error");
        exit(1);
    }

    return old_action.sa_handler;
}

/*************************************************************
 * The Sio (Signal-safe I/O) package - simple reentrant output
 * functions that are safe for signal handlers.
 *************************************************************/

/* Private sio functions */

/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[], size_t len) {
    size_t i, j;
    for (i = 0, j = len - 1; i < j; i++, j--) {
        char c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* write_digits - write digit values of v in base b to string */
static size_t write_digits(uintmax_t v, char s[], unsigned char b) {
    size_t i = 0;
    do {
        unsigned char c = v % b;
        if (c < 10) {
            s[i++] = (char)(c + '0');
        } else {
            s[i++] = (char)(c - 10 + 'a');
        }
    } while ((v /= b) > 0);
    return i;
}

/* Based on K&R itoa() */
/* intmax_to_string - Convert an intmax_t to a base b string */
static size_t intmax_to_string(intmax_t v, char s[], unsigned char b) {
    bool neg = v < 0;
    size_t len;

    if (neg) {
        len = write_digits((uintmax_t)-v, s, b);
        s[len++] = '-';
    } else {
        len = write_digits((uintmax_t)v, s, b);
    }

    s[len] = '\0';
    sio_reverse(s, len);
    return len;
}

/* uintmax_to_string - Convert a uintmax_t to a base b string */
static size_t uintmax_to_string(uintmax_t v, char s[], unsigned char b) {
    size_t len = write_digits(v, s, b);
    s[len] = '\0';
    sio_reverse(s, len);
    return len;
}

/* Public Sio functions */

/**
 * @brief   Prints formatted output to stdout.
 * @param fmt   The format string used to determine the output.
 * @param ...   The arguments for the format string.
 * @return      The number of bytes written, or -1 on error.
 *
 * @remark   This function is async-signal-safe.
 * @see      sio_vdprintf
 */
ssize_t sio_printf(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    ssize_t ret = sio_vdprintf(STDOUT_FILENO, fmt, argp);
    va_end(argp);
    return ret;
}

/**
 * @brief   Prints formatted output to a file descriptor.
 * @param fileno   The file descriptor to print output to.
 * @param fmt      The format string used to determine the output.
 * @param ...      The arguments for the format string.
 * @return         The number of bytes written, or -1 on error.
 *
 * @remark   This function is async-signal-safe.
 * @see      sio_vdprintf
 */
ssize_t sio_dprintf(int fileno, const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    ssize_t ret = sio_vdprintf(fileno, fmt, argp);
    va_end(argp);
    return ret;
}

/**
 * @brief   Prints formatted output to STDERR_FILENO.
 * @param fmt      The format string used to determine the output.
 * @param ...      The arguments for the format string.
 * @return         The number of bytes written, or -1 on error.
 *
 * @remark   This function is async-signal-safe.
 * @see      sio_vdprintf
 */
ssize_t sio_eprintf(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    ssize_t ret = sio_vdprintf(STDERR_FILENO, fmt, argp);
    va_end(argp);
    return ret;
}

struct _format_data {
    const char *str; // String to output
    size_t len;      // Length of string to output
    char buf[128];   // Backing buffer to use for conversions
};

static size_t _handle_format(const char *fmt, va_list argp,
                             struct _format_data *data) {
    size_t pos = 0;
    bool handled = false;

    if (fmt[0] == '%') {
        // Marked if we need to convert an integer
        char convert_type = '\0';
        union {
            uintmax_t u;
            intmax_t s;
        } convert_value = {.u = 0};

        switch (fmt[1]) {

        // Character format
        case 'c': {
            data->buf[0] = (char)va_arg(argp, int);
            data->buf[1] = '\0';
            data->str = data->buf;
            data->len = 1;
            handled = true;
            pos += 2;
            break;
        }

        // String format
        case 's': {
            data->str = va_arg(argp, char *);
            if (data->str == NULL) {
                data->str = "(null)";
            }
            data->len = strlen(data->str);
            handled = true;
            pos += 2;
            break;
        }

        // Escaped %
        case '%': {
            data->str = fmt;
            data->len = 1;
            handled = true;
            pos += 2;
            break;
        }

        // Pointer type
        case 'p': {
            void *ptr = va_arg(argp, void *);
            if (ptr == NULL) {
                data->str = "(nil)";
                data->len = strlen(data->str);
                handled = true;
            } else {
                convert_type = 'p';
                convert_value.u = (uintmax_t)(uintptr_t)ptr;
            }
            pos += 2;
            break;
        }

        // Int types with no format specifier
        case 'd':
        case 'i':
            convert_type = 'd';
            convert_value.s = (intmax_t)va_arg(argp, int);
            pos += 2;
            break;
        case 'u':
        case 'x':
        case 'o':
            convert_type = fmt[1];
            convert_value.u = (uintmax_t)va_arg(argp, unsigned);
            pos += 2;
            break;

        // Int types with size format: long
        case 'l': {
            switch (fmt[2]) {
            case 'd':
            case 'i':
                convert_type = 'd';
                convert_value.s = (intmax_t)va_arg(argp, long);
                pos += 3;
                break;
            case 'u':
            case 'x':
            case 'o':
                convert_type = fmt[2];
                convert_value.u = (uintmax_t)va_arg(argp, unsigned long);
                pos += 3;
                break;
            }
            break;
        }

        // Int types with size format: size_t
        case 'z': {
            switch (fmt[2]) {
            case 'd':
            case 'i':
                convert_type = 'd';
                convert_value.s = (intmax_t)(uintmax_t)va_arg(argp, size_t);
                pos += 3;
                break;
            case 'u':
            case 'x':
            case 'o':
                convert_type = fmt[2];
                convert_value.u = (uintmax_t)va_arg(argp, size_t);
                pos += 3;
                break;
            }
            break;
        }
        }

        // Convert int type to string
        switch (convert_type) {
        case 'd':
            data->str = data->buf;
            data->len = intmax_to_string(convert_value.s, data->buf, 10);
            handled = true;
            break;
        case 'u':
            data->str = data->buf;
            data->len = uintmax_to_string(convert_value.u, data->buf, 10);
            handled = true;
            break;
        case 'x':
            data->str = data->buf;
            data->len = uintmax_to_string(convert_value.u, data->buf, 16);
            handled = true;
            break;
        case 'o':
            data->str = data->buf;
            data->len = uintmax_to_string(convert_value.u, data->buf, 8);
            handled = true;
            break;
        case 'p':
            strcpy(data->buf, "0x");
            data->str = data->buf;
            data->len =
                uintmax_to_string(convert_value.u, data->buf + 2, 16) + 2;
            handled = true;
            break;
        }
    }

    // Didn't match a format above
    // Handle block of non-format characters
    if (!handled) {
        data->str = fmt;
        data->len = 1 + strcspn(fmt + 1, "%");
        pos += data->len;
    }

    return pos;
}

/**
 * @brief   Prints formatted output to a file descriptor from a va_list.
 * @param fileno   The file descriptor to print output to.
 * @param fmt      The format string used to determine the output.
 * @param argp     The arguments for the format string.
 * @return         The number of bytes written, or -1 on error.
 *
 * @remark   This function is async-signal-safe.
 *
 * This is a reentrant and async-signal-safe implementation of vdprintf, used
 * to implement the associated formatted sio functions.
 *
 * This function writes directly to a file descriptor (using the `rio_writen`
 * function from csapp), as opposed to a `FILE *` from the standard library.
 * However, since these writes are unbuffered, this is not very efficient, and
 * should only be used when async-signal-safety is necessary.
 *
 * The only supported format specifiers are the following:
 *  -  Int types: %d, %i, %u, %x, %o (with size specifiers l, z)
 *  -  Others: %c, %s, %%, %p
 */
ssize_t sio_vdprintf(int fileno, const char *fmt, va_list argp) {
    size_t pos = 0;
    ssize_t num_written = 0;

    while (fmt[pos] != '\0') {
        // Int to string conversion
        struct _format_data data;
        memset(&data, 0, sizeof(data));

        // Handle format characters
        pos += _handle_format(&fmt[pos], argp, &data);

        // Write output
        if (data.len > 0) {
            ssize_t ret = rio_writen(fileno, (const void *)data.str, data.len);
            if (ret < 0 || (size_t)ret != data.len) {
                return -1;
            }
            num_written += data.len;
        }
    }

    return num_written;
}

/* Async-signal-safe assertion support*/
void __sio_assert_fail(const char *assertion, const char *file,
                       unsigned int line, const char *function) {
    sio_dprintf(STDERR_FILENO, "%s: %s:%u: %s: Assertion `%s' failed.\n",
                __progname, file, line, function, assertion);
    abort();
}

/***************************************************
 * Wrappers for dynamic storage allocation functions
 ***************************************************/

void *Malloc(size_t size) {
    void *p;

    if ((p = malloc(size)) == NULL) {
        perror("Malloc error");
        exit(1);
    }

    return p;
}

void *Realloc(void *ptr, size_t size) {
    void *p;

    if ((p = realloc(ptr, size)) == NULL) {
        perror("Realloc error");
        exit(1);
    }

    return p;
}

void *Calloc(size_t nmemb, size_t size) {
    void *p;

    if ((p = calloc(nmemb, size)) == NULL) {
        perror("Calloc error");
        exit(1);
    }

    return p;
}

void Free(void *ptr) {
    free(ptr);
}

/****************************************
 * The Rio package - Robust I/O functions
 ****************************************/

/*
 * rio_readn - Robustly read n bytes (unbuffered)
 */
ssize_t rio_readn(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nread = read(fd, bufp, nleft)) < 0) {
            if (errno != EINTR) {
                return -1; /* errno set by read() */
            }

            /* Interrupted by sig handler return, call read() again */
            nread = 0;
        } else if (nread == 0) {
            break; /* EOF */
        }
        nleft -= (size_t)nread;
        bufp += nread;
    }
    return (ssize_t)(n - nleft); /* Return >= 0 */
}

/*
 * rio_writen - Robustly write n bytes (unbuffered)
 */
ssize_t rio_writen(int fd, const void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nwritten;
    const char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno != EINTR) {
                return -1; /* errno set by write() */
            }

            /* Interrupted by sig handler return, call write() again */
            nwritten = 0;
        }
        nleft -= (size_t)nwritten;
        bufp += nwritten;
    }
    return (ssize_t)n;
}

/*
 * rio_read - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n) {
    size_t cnt;

    while (rp->rio_cnt <= 0) { /* Refill if buf is empty */
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0) {
            if (errno != EINTR) {
                return -1; /* errno set by read() */
            }

            /* Interrupted by sig handler return, nothing to do */
        } else if (rp->rio_cnt == 0) {
            return 0; /* EOF */
        } else {
            rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
        }
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;
    if ((size_t)rp->rio_cnt < n) {
        cnt = (size_t)rp->rio_cnt;
    }
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return (ssize_t)cnt;
}

/*
 * rio_readinitb - Associate a descriptor with a read buffer and reset buffer
 */
void rio_readinitb(rio_t *rp, int fd) {
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}

/*
 * rio_readnb - Robustly read n bytes (buffered)
 */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nread = rio_read(rp, bufp, nleft)) < 0) {
            return -1; /* errno set by read() */
        } else if (nread == 0) {
            break; /* EOF */
        }
        nleft -= (size_t)nread;
        bufp += nread;
    }
    return (ssize_t)(n - nleft); /* return >= 0 */
}

/*
 * rio_readlineb - Robustly read a text line (buffered)
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) {
    size_t n;
    ssize_t rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) {
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n') {
                n++;
                break;
            }
        } else if (rc == 0) {
            if (n == 1) {
                return 0; /* EOF, no data read */
            } else {
                break; /* EOF, some data was read */
            }
        } else {
            return -1; /* Error */
        }
    }
    *bufp = 0;
    return (ssize_t)(n - 1);
}

/********************************
 * Client/server helper functions
 ********************************/
/*
 * open_clientfd - Open connection to server at <hostname, port> and
 *     return a socket descriptor ready for reading and writing. This
 *     function is reentrant and protocol-independent.
 *
 *     On error, returns:
 *       -2 for getaddrinfo error
 *       -1 with errno set for other errors.
 */
int open_clientfd(const char *hostname, const char *port) {
    int clientfd = -1, rc;
    struct addrinfo hints, *listp, *p;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM; /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV; /* ... using a numeric port arg. */
    hints.ai_flags |= AI_ADDRCONFIG; /* Recommended for connections */
    if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port,
                gai_strerror(rc));
        return -2;
    }

    /* Walk the list for one that we can successfully connect to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (clientfd < 0) {
            continue; /* Socket failed, try the next */
        }

        /* Connect to the server */
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) {
            break; /* Success */
        }

        /* Connect failed, try another */
        if (close(clientfd) < 0) {
            fprintf(stderr, "open_clientfd: close failed: %s\n",
                    strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) { /* All connects failed */
        return -1;
    } else { /* The last connect succeeded */
        return clientfd;
    }
}

/*
 * open_listenfd - Open and return a listening socket on port. This
 *     function is reentrant and protocol-independent.
 *
 *     On error, returns:
 *       -2 for getaddrinfo error
 *       -1 with errno set for other errors.
 */
int open_listenfd(const char *port) {
    struct addrinfo hints, *listp, *p;
    int listenfd = -1, rc, optval = 1;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
    if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port,
                gai_strerror(rc));
        return -2;
    }

    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listenfd < 0) {
            continue; /* Socket failed, try the next */
        }

        /* Eliminates "Address already in use" error from bind */
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                   sizeof(int));

        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
            break; /* Success */
        }

        if (close(listenfd) < 0) { /* Bind failed, try the next */
            fprintf(stderr, "open_listenfd close failed: %s\n",
                    strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) { /* No address worked */
        return -1;
    }

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0) {
        close(listenfd);
        return -1;
    }
    return listenfd;
}

int enlever_HTTP(int fd) {
    char buffer[256];

    while (1) {
        rio_readn(fd,buffer,1);
        if (buffer[0]=='<') {
            printf("%c",'<');
            break;
        }
    }
    while (rio_readn(fd,buffer,sizeof(buffer)>0)) {
        printf("%s",buffer);
    }
}


int verification_lien(char* lien) {  // prend une liste et renvoie si elle est du type voulu par Q3
    int i=0;
    int k=0;
    char lien_epure[8192];
    while (lien[i] !='\0' && lien[i]!='=' && lien[i]!='>') {
        lien_epure[k]=lien[i];
        i++;
        if (lien[i]==' ' || lien[i]=='\n') {
            k++;
            lien_epure[k]= ' ';
            while (lien[i]==' ' || lien[i]=='\n') {
                i++;
            }
        }
        k++;

    }
    lien_epure[k]='\0';
    if (strcasecmp("<a href",lien_epure) == 0) {
        return 1;
    } 
    return 0;
}


void affiche_lien_Q3(int fd) { //reprend la Q2 mais la logique est revue pour que ça fasse bien ce qui est voulu | pas plus de 250 espaces marchent | 
    char buffer[256];       
    char lien[256];
    int a_print = 0;
    int print_a_partir_de = 0;
    int commencer_verif =0;
    int k=0;
    int print_au_milieu_changement_buffer=0;



    while (rio_readn(fd, buffer, sizeof(buffer)) > 0) {
        for (int i = 0; i < 256; i++) {
            if (print_au_milieu_changement_buffer) {
                printf("%c",buffer[i]);
                if (buffer[i+1]== '\"') {
                    printf("\n");
                    print_au_milieu_changement_buffer=0;
                }
            }
            if (buffer[i]=='<') {
                k=i;
                commencer_verif=1;
                for (int j = 0; j < 256; j++) {
                    lien[j] = '\0';
                }
            }
            if (commencer_verif) {
                lien[i-k]=buffer[i];
                if (i==255) {
                    k-=i;
                }
                if (buffer[i]=='=' || buffer[i]=='>') {
                    if (verification_lien(lien)) {
                        a_print=1;
                        print_a_partir_de=i+2;
                    }
                    commencer_verif=0;
                }
            }

            if (a_print) {
                k = print_a_partir_de;
                while (buffer[k] != ' ' && buffer[k] != '>' && buffer[k] != '\"') {
                    printf("%c", buffer[k]);
                    if (k == 255) {
                        print_au_milieu_changement_buffer=1;
                        commencer_verif=0;   
                        a_print=0;
                        break;
                    } else {
                        k++;
                    }
                }
                if (print_au_milieu_changement_buffer) {
                    break;
                }
                printf("\n");
                a_print = 0;
               
            }
           
            
        }
    }
}

char** affiche_lien_Q4(int fd, int* size) { //reprend la Q3 mais la logique est revue pour que ça fasse bien ce qui est voulu | pas plus de 250 espaces marchent | 
    char buffer[TAILLE_BUFFER];       
    char lien[TAILLE_BUFFER];
    int a_print = 0;
    int print_a_partir_de = 0;
    int commencer_verif =0;
    int k=0;
    int print_au_milieu_changement_buffer=0;
    char** Q4 = (char**)malloc(TAILLE_BUFFER*sizeof(char*));
    for (int l=0;l<TAILLE_BUFFER;l++) {
        Q4[l]=(char*)malloc(TAILLE_BUFFER*sizeof(char));
    }
    int numero_lien=0;
    int indice_dans_le_lien=0;


    while (rio_readn(fd, buffer, sizeof(buffer)) > 0) {
        for (int i = 0; i < TAILLE_BUFFER; i++) {
            if (print_au_milieu_changement_buffer) {
                //printf("%c",buffer[i]);
                indice_dans_le_lien++;
                Q4[numero_lien][indice_dans_le_lien]=buffer[i];
                //printf("%s", Q4[numero_lien]);
                if (buffer[i+1]== '\"') {
                    //printf("\n");
                    print_au_milieu_changement_buffer=0;
                    Q4[numero_lien][indice_dans_le_lien+1]='\0';
                    indice_dans_le_lien=0;
                    numero_lien++;
                }
            }
            if (buffer[i]=='<') {
                k=i;
                commencer_verif=1;
                for (int j = 0; j < TAILLE_BUFFER; j++) {
                    lien[j] = '\0';
                }
            }
            if (commencer_verif) {
                lien[i-k]=buffer[i];
                if (i==TAILLE_BUFFER -1) {
                    k-=i;
                }
                if (buffer[i]=='=' || buffer[i]=='>') {
                    if (verification_lien(lien)) {
                        a_print=1;
                        print_a_partir_de=i+2;
                    }
                    commencer_verif=0;
                }
            }

            if (a_print) {
                k = print_a_partir_de;
                while (buffer[k] != ' ' && buffer[k] != '>' && buffer[k] != '\"') {
                    //printf("%c", buffer[k]);
                    Q4[numero_lien][indice_dans_le_lien] =buffer[k];
                    if (k == TAILLE_BUFFER -1) {
                        print_au_milieu_changement_buffer=1;
                        commencer_verif=0;   
                        a_print=0;
                        break;
                    } else {
                        k++;
                        indice_dans_le_lien++;
                    }
                }

                //printf("\n ici : %s \n",Q4[numero_lien]);
                if (print_au_milieu_changement_buffer) {
                    break;
                }
                Q4[numero_lien][indice_dans_le_lien]='\0';
                //printf("\n");
                a_print = 0;
                numero_lien++;
                indice_dans_le_lien=0;
            }
            
        }
    }
    for (int l=numero_lien+1;l<TAILLE_BUFFER;l++) {
        free(Q4[l]);
    }
    *size=numero_lien;
    return Q4;
}

void URL_decomp(char* lien, char* protocole, char* nom_domaine, char* page) {
    if (lien[0]=='h') { //y'aura que du http ou https
        sscanf(lien, "%9[^:]://%99[^/]/%199[^\n]", protocole, nom_domaine, page);
    } else {
        strcpy(protocole,"http");
        strcpy(nom_domaine, "people.irisa.fr");
        strcpy(page,"/Martin.Quinson/");
        strcat(page,lien);
    }
}



int detection_liens_casses(int fd) {
    int* size_matrice;
    char** matrice_des_liens = affiche_lien_Q4(fd,size_matrice);
    int fd2;
    printf("%d \n", *size_matrice);
    char protocole[20];
    char nom_domaine[100];
    char page[200];
    for (int i=0;i<*size_matrice;i++) {
        URL_decomp(matrice_des_liens[i],protocole, nom_domaine, page);
        int cfd = open_clientfd(nom_domaine,"80");
        char lien2[300]= "GET ";
        strcat(lien2, page);
        strcat(lien2," HTTP/1.0\r\nHost: ");
        strcat(lien2,nom_domaine);
        strcat(lien2,"\r\n\r\n");
        printf("Voici le lien dans rio_writen : %s \n", lien2);
        rio_writen(cfd,lien2,strlen(lien2));

    }
}










void print_matrix(char** arr, int size_arr) {
    for (int i=0;i<size_arr;i++) {
        printf("%s\n",arr[i]);
    }
}



int main() {
    int client_fd = open_clientfd("people.irisa.fr","80");
    rio_writen(client_fd,"GET /Martin.Quinson/ HTTP/1.0\r\nHost: people.irisa.fr\r\n\r\n",strlen("GET /Martin.Quinson/ HTTP/1.0\r\nHost: people.irisa.fr\r\n\r\n"));

    if (client_fd >= 0) {
        
        //enlever_HTTP(client_fd); //cette fonction fait la question 1
        //affiche_lien_Q3(client_fd); //cette fonction fait la question 2 et 3
        //detection_liens_casses(client_fd); //cette fonction ne marche pas, mais devrait faire la question 4 normalement. J'ai pas réussi a continuer a partir du moment oule meme code renvoie plusieurs trucs différents quand je l'appelle.


    }
    close(client_fd);
    return 0;
}











