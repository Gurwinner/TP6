#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 500

/* README Serveur simule un serveur en attente de requêtes pour un lancer de dé. Pour le faire fonctionner, on
compile donc Serveur dans un terminal (le nom de l'éxécutable obtenu est pris dans la suite comme Serveur) 
puis on lance ./Serveur 3549 car on se branche sur le port 3549. 
Le code suivant est largement inspiré du manuel de getaddrinfo.*/

int
main(int argc, char *argv[])
{
    int                      sfd, s;
    u_int8_t                 buf[BUF_SIZE];
    ssize_t                  nread;
    socklen_t                peer_addrlen;
    struct addrinfo          hints;
    struct addrinfo          *result, *rp;
    struct sockaddr_storage  peer_addr;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s port\n", argv[0]);
	exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo("localhost", argv[1], &hints, &result);
    if (s != 0) {
	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
	exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully bind(2).
       If socket(2) (or bind(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
	sfd = socket(rp->ai_family, rp->ai_socktype,
		     rp->ai_protocol);
	if (sfd == -1)
	    continue;

	if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
	    break;                  /* Success */

	close(sfd);
    }

    freeaddrinfo(result);           /* No longer needed */

    if (rp == NULL) {               /* No address succeeded */
	fprintf(stderr, "Could not bind\n");
	exit(EXIT_FAILURE);
    }

    printf("Successfuly bind, waiting for client!\n");
    
    /* Read datagrams and echo them back to sender. */

    printf("Le nombre de faces maximum supporté est %d\n", RAND_MAX); //Car on ne peut représenter d'entiers plus grands en mémoire.

    for (;;) {
	char host[NI_MAXHOST], service[NI_MAXSERV];

	peer_addrlen = sizeof(peer_addr);
	nread = recvfrom(sfd, buf, BUF_SIZE, 0,
			 (struct sockaddr *) &peer_addr, &peer_addrlen);
	if (nread == -1){
	    continue;               /* Ignore failed request */
    }
	
	s = getnameinfo((struct sockaddr *) &peer_addr,
			peer_addrlen, host, NI_MAXHOST,
			service, NI_MAXSERV, NI_NUMERICSERV);
	if (s == 0)
	    printf("Received %zd bytes from %s:%s\n",
		   nread, host, service);
	else
	    fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

    if (buf[0]!=0xde){
        u_int8_t t[2];
        t[0]=0xdf;
        t[1]=0;
    	if (sendto(sfd, t, 2, 0, (struct sockaddr *) &peer_addr,
		   peer_addrlen) != nread)
	    {
		fprintf(stderr, "Error sending response\n");
	    }        
        continue;
    }

    /*u_int8_t ppcm=lcm(RAND_MAX+1,buf[1]); On devrait prendre le ppcm avec RAND_MAX+1 
    car rand donne un tirage sur [0,RAND_MAX], donc de taille RAND_MAX+1, mais l'ordinateur ne peut supporter d'entier plus grand que RAND_MAX.
    On se donne donc juste ppcm de manière théorique pour l'explication de mon idée de procédé.*/

    /*Je n'ai pas réussi ici à simuler un tirage aléatoire entre 1 et nb_face(buf[1]). Si j'arrivais à simuler
    un tirage aléatoire entre 1 et ppcm, alors (reste par division euclienne par nb_face)+1 
    suivrait la loi uniforme sur [1,nb_face]. Cependant, je n'arrive pas à me ramener de tirages uniformes sur [0,RAND_MAX]
    à un tirage uniforme sur [1,ppcm]. Puisque toutes les autres solutions dont j'ai pu parler avec mes camarades parler ne me semblent
    pas donner de tirages uniforme, j'ai décidé de donner simplement le résultat d'un tirage sur [0,RAND_MAX].*/

    u_int8_t lancer=rand();
    u_int8_t t[2];
    t[0]=0xd0;
    t[1]=lancer;

	if (sendto(sfd, t, 2, 0, (struct sockaddr *) &peer_addr,
		   peer_addrlen) != nread)
	    {
		fprintf(stderr, "Error sending response\n");
	    }
    }
}
