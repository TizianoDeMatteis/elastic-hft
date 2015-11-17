/*    ---------------------------------------------------------------------

    Copyright (C) 2015- by Tiziano De Matteis (dematteis <at> di.unipi.it)

    This file is part of elastic-hft.

    elastic-hft is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    ---------------------------------------------------------------------


 */

 //Include:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include "../includes/general.h"
 #include <sys/un.h>



//Funzione per creare una connessione TCP verso un IP address e una porta (e restituire il socket):
int connect_to(const char *ip_address, int port) {

	#if defined(USE_AF_UNIX)
	struct sockaddr_un sockAddress;
	sockAddress.sun_family = AF_UNIX;
	strcpy(sockAddress.sun_path,"./socket");
	int s;
	if((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("Error opening a UNIX socket");
		exit(-1);
	}
	#else
	//Preparo la struttura sockaddr_in:
	struct sockaddr_in sockAddress;
	sockAddress.sin_family = AF_INET;
	sockAddress.sin_port = htons(port);
	sockAddress.sin_addr.s_addr = inet_addr(ip_address);
	//Controllo se anzichè l'IP è stato fornito un hostname:
	struct hostent *hostEntity;
	char hnamebuf[MAXHOSTNAMELEN];
	if(sockAddress.sin_addr.s_addr == (u_int) -1) {
		//Considero address un hostname e provo a tradurlo:
		hostEntity = gethostbyname(ip_address);
		if(!hostEntity) {
			perror("Error in the resolution of the provided hostname");
			exit(-1);
		}
		else {
			sockAddress.sin_family = hostEntity->h_addrtype;
			bcopy(hostEntity->h_addr, (caddr_t)&sockAddress.sin_addr, hostEntity->h_length);
			strncpy(hnamebuf, hostEntity->h_name, sizeof(hnamebuf) - 1);
		
		}		
	}	
	//Apertura socket TCP:
	int s;
	if((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("Error opening a TCP socket");
		exit(-1);
	}
	#endif
	//Eseguo la connect per collegarmi all'ip_address e alla porta:
	if(connect(s, (struct sockaddr*) &sockAddress, sizeof(struct sockaddr)) < 0) {
		printf("Error connecting to Farm with IP %s and Port %d\n", ip_address, port);
		exit(-1);
	}
	int size=1048576;
	setsockopt(s, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int));
	int n;
	unsigned int m = sizeof(n);
	getsockopt(s,SOL_SOCKET,SO_SNDBUF,(void *)&n, &m);


	return s;
}

//Funzione per ricevere num_conn connessioni TCP su una porta (ritorna un array di socket):
int * receive_connection(int num_conn, int port) {
	if(num_conn > 0) {

		#if defined(USE_AF_UNIX)
		//remove previous socket
		unlink("./socket");
		struct sockaddr_un serverSocketAddress;
		struct sockaddr_un partnerAddr;
		socklen_t sockaddr_len = (socklen_t) sizeof(partnerAddr);
		
		serverSocketAddress.sun_family = AF_UNIX;
		strcpy(serverSocketAddress.sun_path,"./socket");
		int serverSocket;
		if((serverSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
			perror("Error opening a UNIX socket");
			exit(-1);
		}
		//Bind del socket:
		if(bind(serverSocket, (struct sockaddr *) &serverSocketAddress, sizeof(serverSocketAddress)) < 0) {
			perror("Error bind() call");
			exit(-1);
		}
		//Listen sul server socket:
		if(listen(serverSocket, 1) < 0) {
			perror("Error list() call");
			exit(-1);
		}
		printf("AF UNIX\n");
		#else
		//Preparo la struttura sockaddr_in:
		struct sockaddr_in serverSocketAddress;
		struct sockaddr_in partnerAddr;
		socklen_t sockaddr_len = (socklen_t) sizeof(partnerAddr);
		serverSocketAddress.sin_family = AF_INET;
		serverSocketAddress.sin_port = htons(port);
		serverSocketAddress.sin_addr.s_addr = INADDR_ANY;
		//Apertura socket TCP:
		int serverSocket;
		if((serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			perror("Error opening a TCP socket");
			exit(-1);
		}
		//Opzione per riusare la porta local senza attendere il TCP timeout:
		int yes = 1;
		setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		
		//Bind del socket:
		if(bind(serverSocket, (struct sockaddr *) &serverSocketAddress, sizeof(serverSocketAddress)) < 0) {
			perror("Error bind() call");
			exit(-1);
		}
		//Listen sul server socket:
		if(listen(serverSocket, 1) < 0) {
			perror("Error list() call");
			exit(-1);
		}
		#endif
		//Ciclo in cui attendo tutte le num_conn:
		int *s = (int *) malloc(sizeof(int) * num_conn);
		for(int i = 0; i<num_conn; i++) {
			if((s[i] = accept(serverSocket, (struct sockaddr *) &partnerAddr, &sockaddr_len)) < 0) {
				perror("Errore accept() call");
				exit(-1);
			}
		//int size=1048576; QUESTE SONO LA MORTE
		//setsockopt(s[i], SOL_SOCKET, SO_RCVBUF, &size, sizeof(int));
		}
		return s;
	}
	else return NULL;
}



//Funzione per selezionare non-deterministicamente un Socket Descriptor da cui ricevere:
int selectConnection(fd_set *fds, int *s_array, int nConnections, int lastSocket, int max) {
	fd_set readfds;
	int ready;
	//Copio l'insieme dei flag che di volta in volta viene resettato dalla select:
	readfds = *fds;
	//Faccio la select finchè almeno un socket ha qualche dato da ricevere:
	do {
		//Eseguo la select non-deterministica:
		ready = select(max + 1, &readfds, NULL, NULL, NULL);
	} while(ready == -1);
	//C'è almeno un socket su cui si può ricevere:
	bool trovato = false;
	//Scorro i socket per trovare il primo non vuoto (politica di scansione circolare fair):
	int index = (lastSocket + 1) % nConnections;
	int count = 0;
	while((!trovato) && (count < nConnections)) {
		//Controllo se il socket ha dati:
		if(FD_ISSET(s_array[index], &readfds)) trovato = true;
		else {
			index = (index + 1) % nConnections;
			count++;
		}
	}
	if(trovato) return index;
	else return -1;
}

int closeSocket(int socket)
{
	
	return close(socket);
}

/** Returns true on success, or false if there was an error */
bool SetSocketBlockingEnabled(int fd, bool blocking)
{
   if (fd < 0) return false;


   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return false;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;

}
