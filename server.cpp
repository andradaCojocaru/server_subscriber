#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "helpers.h"
#include <vector>
#include <map>
#include <iostream>
#include <math.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std;

// structura pentru pachetele primite de la udp
struct udp_packet {
	string ip_udp;
	int port_udp;
	char topic[TOPIC_LEN + 1];
	char content[CONTENT_LEN + 1];
	uint8_t type;
};

// structura unui utilizator
struct user {
	int fd;
	bool connected;
	string ip_tcp;
	int port_tcp;
	string id;
};

// structura unui subscriber
struct subscriber {
	// la baza un subscriber este un user, asa ca ma folosesc de implementarea
	// sa
	struct user MyUser;
	vector<pair<bool, string>> topics;
	vector<udp_packet> packetsStored;
};

map<int, subscriber> subscribers;
vector<user> users;
vector<udp_packet> packets;

// logica creare server din laborator
void do_logic(fd_set &read_fds, fd_set &tmp_fds, int &sockTcp, int &sockUdp,
	int &portno, char *argv[], int &ret, struct sockaddr_in &serv_addr)
{
	// se goleste multimea de descriptori de citire (read_fds)
	// si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockTcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockTcp < 0, "socketTCP");

    // dezactivam neagle
    int flag = 1;
    int result = setsockopt(sockTcp, IPPROTO_TCP, TCP_NODELAY,
		(char *)&flag, sizeof(int));
    DIE(result < 0, "disable neagle");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockTcp, (struct sockaddr *) &serv_addr,
		sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockTcp, MAX_CLIENTS);
	DIE(ret < 0, "listen");

    sockUdp = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sockUdp < 0, "socketUDP");

    ret = bind(sockUdp, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "bind");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni)
	// in multimea read_fds
	FD_SET(0, &read_fds);
	FD_SET(sockTcp, &read_fds);
    FD_SET(sockUdp, &read_fds);
}

// in cazul in care primesc comenzi de la tastatura
int handle_stdin(char *buffer, int &ret)
{
	memset(buffer, 0, MAX_LEN);
	fgets(buffer, MAX_LEN, stdin);
	buffer[strlen(buffer) - 1] = 0;

	// comanda exit este singura valida pentru a realiza o actiune				
	if (strcmp(buffer, "exit") == 0) {
		for (int j = 0; j < (int)users.size(); j++) {
			if (users[j].connected == true) {
				memset(buffer, 0, MAX_LEN);
				strcpy(buffer, "EXIT_SERVER");
				ret = send(users[j].fd, buffer, strlen(buffer), 0);
				DIE(ret < 0, "send err exit");
				close(users[j].fd);
			}
		}
		users.clear();
		return 1;
	}
	return 0;
}

// parsam informatiile din pachetul primit
void get_info_packet(char *topic, char *content, uint8_t &type, char *buffer)
{
	memcpy(topic, buffer, TOPIC_LEN);
	memcpy(&type, buffer + TOPIC_LEN, sizeof(uint8_t));
	memcpy(content, buffer + TOPIC_LEN + 1, CONTENT_LEN);
}

// cream mesajul pe care trebuie sa il trimitem
string handle_udp_packet(char *content, uint8_t type,
	string ip_udp, int port_udp, char *topic)
{
	stringstream s;
	s << ip_udp << ":" << port_udp << " - " << topic << " - ";

	switch(type) {
		// caz INT
		case 0: {
			uint8_t sign0;
			uint32_t value0;

			memcpy(&sign0, content, sizeof(uint8_t));
			memcpy(&value0, content + 1, sizeof(uint32_t));

			if (sign0 == 1) {
				s << "INT - -" << ntohl(value0) <<"\n";
			} else {
				s << "INT - " << ntohl(value0) <<"\n";
			}
		} break;
		// caz SHORT REAL
		case 1: {
			uint16_t value1;

			memcpy(&value1, content, sizeof(uint16_t));
			s << "SHORT_REAL - " << fixed << setprecision(2)
				<< (double)ntohs(value1) / 100 <<"\n";
		} break;
		// caz FLOAT
		case 2: {
			uint8_t sign2, power;
			uint32_t value2;

			memcpy(&sign2, content, sizeof(uint8_t));
			memcpy(&value2, content + 1, sizeof(uint32_t));
			memcpy(&power, content + 5, sizeof(uint8_t));

			double result = (double(ntohl(value2))) * pow(10,
				(-1) * (int) power);
			if (sign2 == 1) {
				s << "FLOAT - -" << fixed << setprecision((int)power) <<
					result <<"\n";
			} else {
				s << "FLOAT - " << fixed << setprecision((int)power) <<
					result <<"\n";
			}
		} break;
		// caz STRING
		case 3: {
			s << "STRING - " << content << "\n";
		} break;
	}
	return s.str();
}

// cerere de la clientul tcp
void handle_tcp_client(struct sockaddr_in &cli_addr, int &newsockfd, int &sockTcp,
	char *buffer, int &fdmax, int &ret, fd_set &read_fds, socklen_t &clilen)
{
	// a venit o cerere de conexiune pe socketul inactiv 
	// (cel cu listen),
	// pe care serverul o accepta
	struct user userConnected;
	clilen = sizeof(cli_addr);
	newsockfd = accept(sockTcp, (struct sockaddr *) &cli_addr,
		&clilen);
	DIE(newsockfd < 0, "accept");

	// se adauga noul socket intors de accept() la multimea
	// descriptorilor de citire
	FD_SET(newsockfd, &read_fds);
	if (newsockfd > fdmax) { 
		fdmax = newsockfd;
	}

	// se primeste pachetul cu ID-ul user-ului
	memset(buffer, 0, MAX_LEN);
	ret = recv(newsockfd, buffer, sizeof(buffer) - 1, 0);

	int ok = 0;
	for (int j = 0; j < (int)users.size() && ok == 0; j++) {
		// daca mai gasim un alt client cu acelasi id
		if (buffer == users[j].id) {
			// daca clientul este conectat
			if (users[j].connected == true) {
				cout << "Client" << " " << users[j].id << " " <<
				"already connected.\n";
				memset(buffer, 0, MAX_LEN);
                strcpy(buffer, "ID_EXISTS\n");
                ret = send(newsockfd, buffer, strlen(buffer), 0);
                DIE(ret < 0, "send err id exists");
				// daca clientul s-a reconectat
			} else {
				users[j].connected = true;
				users[j].port_tcp = ntohs(cli_addr.sin_port);
				subscribers[newsockfd].MyUser.port_tcp =  ntohs(cli_addr.sin_port);
				subscribers[newsockfd].MyUser.connected =  true;
				
				// trimitem pachetele care se afla in lista
				for (int k = 0; k < (int)subscribers[newsockfd]
					.packetsStored.size(); k++) {

					string s = handle_udp_packet(subscribers[newsockfd]
						.packetsStored[k]
						.content, subscribers[newsockfd].packetsStored[k].type,
						subscribers[newsockfd].packetsStored[k].ip_udp,
						subscribers[newsockfd].packetsStored[k].port_udp, 
						subscribers[newsockfd].packetsStored[k].topic);
					
					ret = send(newsockfd, s.c_str(), MAX_LEN, 0);
					DIE(ret < 0, "send udp packet err");
				}
				cout << "New client" << " " << buffer << " " <<
				"connected from" << " " <<
				inet_ntoa(cli_addr.sin_addr) << ":" <<
				ntohs(cli_addr.sin_port) <<".\n";
			}
			ok = 1;
		}
	}
	// altfel conectat un client nou
	if (ok == 0) {
		userConnected.ip_tcp = inet_ntoa(cli_addr.sin_addr);
		userConnected.port_tcp = ntohs(cli_addr.sin_port);
		userConnected.connected = true;
		userConnected.fd = newsockfd;
		userConnected.id = buffer;
		users.push_back(userConnected);

		cout << "New client" << " " << buffer << " " <<
			"connected from" << " " <<
			userConnected.ip_tcp << ":" <<
			userConnected.port_tcp<<".\n";
	}
}

// preluam pachetele venite de la udp
void handle_udp(socklen_t &udplen, struct sockaddr_in &udp_addr, char *buffer,
	int &ret, int &sockUdp)
{
	struct udp_packet packet;

	udplen = sizeof(udp_addr);

	memset(buffer, 0, MAX_LEN);
	ret = recvfrom(sockUdp, buffer, MAX_LEN, 0,
		(struct sockaddr*) &udp_addr, &udplen);
	DIE(ret < 0, "recv");
	
	packet.ip_udp = inet_ntoa(udp_addr.sin_addr);
	packet.port_udp = ntohs(udp_addr.sin_port);

	// preluam informatia din pachete si o copiem in structura de date
	// pe care o vom adauga la lista noastra
	get_info_packet(packet.topic, packet.content, packet.type, buffer);

	// iteram prin lista de subscriberi
	map<int, subscriber>::iterator it;
	for (it = subscribers.begin(); it != subscribers.end(); it++) {
		for (int j = 0; j < (int)it->second.topics.size(); j++) {

			// daca gasim topicul cautat
			if (packet.topic == it->second.topics[j].second) {

				// daca userul este conectat, trimitem pachetul
				if (it->second.MyUser.connected == true) {
					string s = handle_udp_packet(packet.content,
						packet.type, packet.ip_udp, packet.port_udp,
						packet.topic);
					ret = send(it->first, s.c_str(), s.size(), 0);
					DIE(ret < 0, "send err message udp");

				// altfel il punem in lista de pachete in asteptare
				} else {
					if (it->second.topics[j].first == true) {
						it->second.packetsStored.push_back(packet);
					}
				}
			}
		}
	}
}

// inchiderea clientului tcp
void tcp_client_closed(int i)
{
	int ok = 0;
	// parcurgem fiecare client si ii trecem parametrul connected in false
	for (int j = 0; j < (int)users.size() && ok == 0; j++) {
		if (users[j].fd == i && users[j].connected == true) {
			users[j].connected = false;
			subscribers[i].MyUser.connected = false;
			cout << "Client " << users[j].id <<
				" disconnected.\n";
			ok = 1;
		}
	}

	// acelasi procedeu pentru subscribers
	ok = 0;
	for (int j = 0; j < (int)subscribers.size() && ok == 0; j++) {
		if (subscribers[j].MyUser.fd == i) {
			subscribers[j].MyUser.connected = false;
			ok = 1;
		}
	}
}

// clientul este deschis
void tcp_client_open(char *buffer, int i)
{
	char type[MAX_TYPE];
	char topic[TOPIC_LEN];
	int SFint = 0;
	sscanf(buffer, "%s %s %d", type, topic, &SFint);
	// primeste comanda de subscribe
	if (strcmp(type, "subscribe") == 0) {
		bool SF;
		int ok = 0;
		if (SFint == 0) {
			SF = false;
		} else {
			SF = true;
		}

		for (int j = 0; j < (int)users.size(); j++) {
			if (users[j].fd == i) {
				subscribers[i].MyUser = users[j];
			}
		}
		for (int j = 0; j < (int)subscribers[i].topics.size(); j++) {
			if (topic == subscribers[i].topics[j].second) {
				subscribers[i].topics[j].first = SF;
				ok = 1;
			}
		}

		// punem in vectorul de topics a subscriberul perechea de nume, sf
		if (ok == 0) {
			subscribers[i].topics.push_back(make_pair(SF, topic));
		}

	// primim comanda unsubscribe
	} else if (strcmp(type, "unsubscribe") == 0) {
		for (int j = 0; j < (int)subscribers[i].topics.size(); j++) {
			// stergem elementul din lista de topic-uri la care este abonat
			// utilizatorul
			if (topic == subscribers[i].topics[j].second) {
				subscribers[i].topics.erase(subscribers[i].topics.begin() + j);
				j--;
			}
		}
	}
}

// primim informatii de la tcp
void handle_tcp_client_input(char *buffer, int &ret, fd_set &read_fds, int i)
{
	memset(buffer, 0, MAX_LEN);
	ret = recv(i, buffer, MAX_LEN, 0);
	DIE(ret < 0, "recv");
	
	if (ret == 0) {
		// conexiunea s-a inchis
		tcp_client_closed(i);
		close(i);
	
		// se scoate din multimea de citire socketul inchis
		FD_CLR(i, &read_fds);
		
	} else {
		tcp_client_open(buffer, i);
	}
}

int main(int argc, char *argv[]) {
    int sockTcp, sockUdp, portno, newsockfd;
	char buffer[MAX_LEN];
	struct sockaddr_in serv_addr, cli_addr, udp_addr;
	socklen_t clilen, udplen;
	int ret, i;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

    DIE(argc < 2, "Usage server err");

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	do_logic(read_fds, tmp_fds, sockTcp, sockUdp, portno, argv, ret,
		serv_addr);

	fdmax = max(sockTcp, sockUdp);

    while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		
		if (FD_ISSET(0, &tmp_fds)) { // verificam mesajele de la stdin
            int ok = handle_stdin(buffer, ret);
			if (ok == 1) {
				break;
			}
        }

		for (i = 1; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockTcp) {	// client nou
					handle_tcp_client(cli_addr, newsockfd, sockTcp, buffer,
						fdmax, ret, read_fds, clilen);
					
				  // primim informatii de la clientii UDP
				} else if (i == sockUdp) {
					handle_udp(udplen, udp_addr, buffer, ret, sockUdp);
				  // input de la clientii TCP
				} else {
					handle_tcp_client_input(buffer, ret, read_fds, i);
				}
			}
		}
    }

	close(sockUdp);
    close(sockTcp);

    return 0;
}