#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 500

/* README Client simule un client envoyant une requète de lancer à un serveur. Pour le faire fonctionner, on
compile donc Client dans un terminal (le nom de l'éxécutable obtenu est pris dans la suite comme Client) 
après avoir déjà éxécuté Serveur dans un autre, et on lance ./Client localhost 3549 nb_face_demandées car 
on cherche à se brancher au port 3549 de notre propre machine, étant donné que Serveur tourne sur notre propre machine.
Le code suivant est largement inspiré du manuel de getaddrinfo. */

int
main(int argc, char *argv[])
{
    int              sfd, s;
    u_int8_t         buf[BUF_SIZE];
    size_t           len;
    ssize_t          nread;
    struct addrinfo  hints;
    struct addrinfo  *result, *rp;

    if (argc < 3) {
	fprintf(stderr, "Usage: %s host port msg...\n", argv[0]);
	exit(EXIT_FAILURE);
    }

    /* Obtain address(es) matching host/port. */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (s != 0) {
	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
	exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
	sfd = socket(rp->ai_family, rp->ai_socktype,
		     rp->ai_protocol);
	if (sfd == -1)
	    continue;

	if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
	    break;                  /* Success */

	close(sfd);
    }

    freeaddrinfo(result);           /* No longer needed */

    if (rp == NULL) {               /* No address succeeded */
	fprintf(stderr, "Could not connect\n");
	exit(EXIT_FAILURE);
    }

    /* Send remaining command-line arguments as separate
       datagrams, and read responses from server. */
    uint8_t t[2];
    t[0]=0xde;
    t[1]=argv[3][0]-'0'; //On convertit argv en entier. 

	if (write(sfd, t, 2) != 2) {
	    fprintf(stderr, "partial/failed write\n");
	    exit(EXIT_FAILURE);
	}

	nread = read(sfd, buf, BUF_SIZE);

	if (nread == -1||buf[0]!=0xd0) {
	    perror("read");
	    exit(EXIT_FAILURE);
	}

	printf("Received %zd bytes\n", nread);
    printf("Le lancer obtenu est : %d \n",buf[1]);

    exit(EXIT_SUCCESS);
}