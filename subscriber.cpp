#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include "helpers.h"
#include <iostream>
#include <math.h>
#include <algorithm>
#include <iomanip> 

using namespace std;

// trimitem pachetul cu id-ul
void send_id_packet(char *buffer, char *argv[], int &sockTcp)
{
	int ret;
	memset(buffer, 0, MAX_LEN);
    strcpy(buffer, argv[1]);

    ret = send(sockTcp, buffer, strlen(buffer), 0);
    DIE(ret < 0, "send id failed");
}

// implementarea efectiva a clientului din laborator
void do_logic(int &sockTcp, int &ret, struct sockaddr_in &serv_addr,
	fd_set &read_fds, fd_set &tmp_fds, char *buffer, char *argv[])
{
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockTcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockTcp < 0, "socketTCP");

    // dezactivam neagle
    int flag = 1;
    int result = setsockopt(sockTcp, IPPROTO_TCP, TCP_NODELAY, (char *)&flag,
        sizeof(int));
    DIE(result < 0, "disable neagle");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockTcp, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	send_id_packet(buffer, argv, sockTcp);

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockTcp, &read_fds);
}

// input primit de la tastatura
int handle_stdin(char *buffer, int &sockTcp, int &ret)
{
	char type[MAX_TYPE];
	char other[OTHER];
	memset(buffer, 0, MAX_LEN);
	fgets(buffer, MAX_LEN, stdin);
	sscanf(buffer, "%s %s", type, other);
					
	if (strcmp(type, "subscribe") == 0) {
		ret = send(sockTcp, buffer, strlen(buffer), 0);
        DIE(ret < 0, "send subscribe err");
        cout << "Subscribed to topic.\n";
	} else if (strcmp(type, "unsubscribe") == 0) {
		ret = send(sockTcp, buffer, strlen(buffer), 0);
    	DIE(ret < 0,  "send unsubscribe err");
        cout << "Unsubscribed from topic.\n";
	} else if (strcmp(type, "exit") == 0) {
		return 1;
	}
	return 0;
}

// mesaje primite de la server
int handle_tcp(char *buffer, int &sockTcp, int &ret)
{
	char type[MAX_TYPE];
	char other[OTHER];
	memset(buffer, 0, MAX_LEN);
	ret = recv(sockTcp, buffer, MAX_LEN, 0);
	DIE(ret < 0, "recv");
	sscanf(buffer, "%s %s", type, other);

	if (strcmp(type, "EXIT_SERVER") == 0) {
		return 1;
	} else if (strcmp(type, "ID_EXISTS") == 0) {
		return 1;
	}
	cout << buffer;
	return 0;
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc < 4, "usage err");

	int sockTcp, ret;
	struct sockaddr_in serv_addr;
	char buffer[MAX_LEN];
	fd_set read_fds, tmp_fds;

	do_logic(sockTcp, ret, serv_addr, read_fds, tmp_fds, buffer, argv);

    while (1) {
		tmp_fds = read_fds; 
		
		ret = select(sockTcp + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		
		// se citeste de la tastatura
		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			int ok = handle_stdin(buffer, sockTcp, ret);
			if (ok == 1) {
				break;
			}
		}

		// primit pe TCP
		if (FD_ISSET(sockTcp, &tmp_fds)) {
			int ok = handle_tcp(buffer, sockTcp, ret);
			if (ok == 1) {
				break;
			}
		}
	}
	close(sockTcp);

    return 0;
}