//mp0client.c for mp0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>

#include <arpa/inet.h>

#define MAXDATASIZE    		1000 // max number of bytes we can get at once 
#define STRING_PROMPT 		12  //the prompt before each string from server
#define NL_ASCII   			10
#define NAME_BUF      		1000
#define A_ASCII				65
#define LEN_1				5
#define LEN_2 				13
#define LEN_3  				5
#define LEN_4 				4


int main(int argc, char *argv[])
{
	int sockfd, numbytes, rv; 
	int i, j;		//loop counter 
  	int bytes_sent, bytes_recv, recv_length;
	struct addrinfo hints, *servinfo, *p;
  	socklen_t addr_len;
  	char recv_buf[MAXDATASIZE];
	char name_buf[NAME_BUF];

	memset(name_buf, '\0', sizeof(name_buf));
    strcpy(name_buf, "USERNAME = ");


	if (argc ==1) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}else if (argc == 2) {
		fprintf(stderr,"usage: port number\n");
	    exit(1);
	}else if (argc == 3) {
		fprintf(stderr, "usage: USERNAME\n");
		exit(1);
	}else if (argc > 4) {
		fprintf(stderr, "error: too many arguments\n");
		exit(1);
	}

	strcat(name_buf, argv[3]);

	if(strlen(argv[3]) > NAME_BUF - LEN_2) {
		fprintf(stderr, "error: USERNAME too long\n");
		exit(1);
	}


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
	}

	freeaddrinfo(servinfo); // all done with this structure

  	//handshake
  	send(sockfd, "HELO\n", LEN_1, 0);
  	recv(sockfd, recv_buf, MAXDATASIZE-1, 0);
  	send(sockfd, "USERNAME = y\n", LEN_2, 0);
  	recv(sockfd, recv_buf, MAXDATASIZE-1, 0);

  	//print out the received message from server
  	for(i = 0; i < 10; i++){
		send(sockfd, "RECV\n", LEN_3, 0);		
		recv_length = (int)(recv(sockfd, recv_buf, MAXDATASIZE-1, 0));
	  	printf("Received: %.*s", recv_length-STRING_PROMPT, (char*)recv_buf+STRING_PROMPT ); 

	  	while((((char*)recv_buf)[recv_length-1]) != NL_ASCII) {		//check if the string meets end
			recv_length = (int)(recv(sockfd, recv_buf, MAXDATASIZE-1, 0));
		  	printf("%.*s", recv_length, (char*)recv_buf); 
	  	}

	}
  	//bye bye
  	send(sockfd, "BYE\n", LEN_4, 0);		//10
	recv(sockfd, recv_buf, MAXDATASIZE-1, 0);

	close(sockfd);		// close socket descriptor

	return 0;
}

