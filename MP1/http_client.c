/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "80" // the port client will be connecting to 
#define FILE_NAME "output"	//output file
#define NULL_TERMINATOR '\0'

#define MAXDATASIZE 1024 // max number of bytes we can get at once 
#define NL_ASCII   	10

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char recv_buf[MAXDATASIZE];
	int recv_length;
	char* msg_body;
	char ip[1024];
	char port[1024]; 
	int succ_parsing;
	char page[1024];
	char get_msg[1024];
	char new_url[1024];
	char new_page[1024];
	char* new_location;
	FILE* file_ptr;
	int header_size;
	char* file_type;
	char* file_name;


	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	//parse the argument
	succ_parsing = 0;	
	port[0] = NULL_TERMINATOR;
	memset(page, 0, sizeof(page));

	if (sscanf(argv[1], "http://%99[^:]:%99[^/]/%199[^\n]", ip, port, page) == 3) { succ_parsing = 1;}
	else if (sscanf(argv[1], "http://%99[^/]/%199[^\n]", ip, page) == 2) { succ_parsing = 1;}
	else if (sscanf(argv[1], "http://%99[^:]:%99[^\n]", ip, port) == 2) { succ_parsing = 1;}
	else if (sscanf(argv[1], "http://%99[^/\n]", ip) == 1) { succ_parsing = 1;}
	else if (sscanf(argv[1], "http://%99[^\n]", ip) == 1) { succ_parsing = 1;}


	//check parsing status
	if(succ_parsing == 0) {
		printf("inappropriate input\n");
		exit(1);
	}

	// //get file type
	// if(*page == NULL_TERMINATOR) {
	// 	printf("error: no path\n");
	// 	exit(1);
	// } 
	// else {
	// 	file_type = page + strlen(page)-1;
	// 	printf("page length = %d\n", (int)strlen(page));
	// 	while((file_type[0] != '.') && (file_type[0] != '/') && (file_type[0] != NULL_TERMINATOR)) {
	// 		file_type--;
	// 	}
	// 	if(file_type[0] == '.') file_type++;
	// 	else file_type = NULL_TERMINATOR;
	// }
	// printf("file type = %s\n", file_type);

	//for safety
	ip[1023] = NULL_TERMINATOR;
	page[1023] = NULL_TERMINATOR;
	port[1023] = NULL_TERMINATOR;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if(port[0] == NULL_TERMINATOR) {
		if ((rv = getaddrinfo(ip, PORT, &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return 1;
		}
	}
	else {
		if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return 1;
		}		
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
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

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	strcpy(get_msg, "GET /");
	strcat(get_msg, page);
	strcat(get_msg, " HTTP/1.0\r\n");
	strcat(get_msg, "Host: ");
	strcat(get_msg, ip);
	if(port[0] != NULL_TERMINATOR){
		strcat(get_msg, ":");
		strcat(get_msg, port);
	}
	strcat(get_msg, "\r\n\r\n");
	printf("%s\n", get_msg);

	send(sockfd, get_msg, strlen(get_msg), 0);
	//catch message body
	recv_length = (int)(recv(sockfd, recv_buf, MAXDATASIZE-1, 0));
	// printf("%s\n", recv_buf);
	msg_body = strstr(recv_buf, "\r\n\r\n");
	header_size = msg_body - recv_buf;
	// printf("%*.*s\n", header_size, header_size, recv_buf);

	while(strstr(recv_buf, "301 Moved Permanently") != NULL && strstr(recv_buf, "https://") == NULL) {	//handle redirection
		printf("redirecting......\n");
		close(sockfd);
		new_location = strstr(recv_buf, "Location:");
		sscanf(new_location, "Location: %99[^\n]", new_url);
		// printf("new_url = %s\n", new_url);
		memset(recv_buf, 0, strlen(recv_buf));
		// printf("%s\n",new_url );
			//parse the argument
		succ_parsing = 0;	
		port[0] = NULL_TERMINATOR;
		// memset(page, 0, sizeof(page));

		if (sscanf(new_url, "http://%99[^:]:%99[^/]/%199[^\r\n]", ip, port, page) == 3) { succ_parsing = 1;}
		else if (sscanf(new_url, "http://%99[^/]/%199[^\r\n]", ip, page) == 2) { succ_parsing = 1;}
		else if (sscanf(new_url, "http://%99[^:]:%99[^\r\n]", ip, port) == 2) { succ_parsing = 1;}
		else if (sscanf(new_url, "http://%99[^/\r\n]", ip) == 1) { succ_parsing = 1;}
		else if (sscanf(new_url, "http://%99[^\r\n]", ip) == 1) { succ_parsing = 1;}
			//for safety
		// page[strlen(page)-1] = NULL_TERMINATOR;
		// printf("new_page = %s\n", page);
		// printf("len page = %d\n", (int)strlen(page));
		// printf("ip = %s\n", ip);
		// printf("port = %s\n", port);

		ip[1023] = NULL_TERMINATOR;
		page[1023] = NULL_TERMINATOR;
		port[1023] = NULL_TERMINATOR;

		//
		// strcpy(new_page, page);

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		if(port[0] == NULL_TERMINATOR) {
			if ((rv = getaddrinfo(ip, PORT, &hints, &servinfo)) != 0) {
				fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
				return 1;
			}
		}
		else {
			if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
				fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
				return 1;
			}		
		}

		for(p = servinfo; p != NULL; p = p->ai_next) {
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

			break;
		}

		if (p == NULL) {
			fprintf(stderr, "client: failed to connect\n");
			return 2;
		}

		inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
				s, sizeof s);
		printf("client: connecting to %s\n", s);

		freeaddrinfo(servinfo); // all done with this structure

		memset(get_msg, 0, sizeof(get_msg));
		strcpy(get_msg, "GET /");
		// printf("len = %d\n",(int)strlen(get_msg) );
		// printf("len page = %d\n", (int)strlen(page));
		// printf("%s\n", get_msg);
		strcat(get_msg, page);
		// printf("%s\n", get_msg);
		strcat(get_msg, " HTTP/1.0\r\n");
		// printf("%s\n", get_msg);
		// strcat(get_msg, "Accept: */*\r\n");
		strcat(get_msg, "Host: ");
		strcat(get_msg, ip);
		if(port[0] != NULL_TERMINATOR){
			strcat(get_msg, ":");
			strcat(get_msg, port);
		}
		strcat(get_msg, "\r\n\r\n");

		printf("%s\n", get_msg);

		send(sockfd, get_msg, strlen(get_msg), 0);
		recv_length = (int)(recv(sockfd, recv_buf, MAXDATASIZE-1, 0));
		msg_body = strstr(recv_buf, "\r\n\r\n");
		header_size = msg_body - recv_buf;
		// printf("%*.*s\n", header_size, header_size, recv_buf);

	}
	if(msg_body == NULL) {
		printf("no msg return\n");
		exit(1);
	}

	file_ptr = fopen(FILE_NAME, "w");
	
	fprintf(file_ptr, "%.*s", recv_length, (char*)(msg_body+4)); 

	while(recv_length > 0) {		//check if the string meets end
		recv_length = (int)(recv(sockfd, recv_buf, MAXDATASIZE-1, 0));
		fprintf(file_ptr, "%.*s", recv_length, (char*)recv_buf); 
	}

	fclose(file_ptr);
	close(sockfd);

	return 0;
}

