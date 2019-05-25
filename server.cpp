#include <iostream>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include <string.h>

#include "helpers.h"

class Server {
	private:
		int sockUDP; // socket-ul UDP
		int sockTCP; // socket-ul TCP
		socklen_t addrlen;
		struct sockaddr_in server_addr;
		struct sockaddr_in client_addr;
		int opt;
		int status;

    		fd_set read_fds;
		fd_set tmp_fds;
		int fdmax;	
		
		// pentru un client imi spune daca este activ sa nu la un moment de timp
		std::map<std::string, bool> map_clients_alive;

		// pentru fiecare client_id asocieaza un socket(util cand trimitem mesaje catre clienti TCP)
		std::map<std::string, int> map_idClient_socket;

		// pentru fiecare socket asocieaza un client_id(util cand primim mesaje de la clientii TCP)
		std::map<int, std::string> map_socket_idClient;

		// asocieaza pentru fiecare topic o lista de perechi de forma (id_client, SF=0/1)
		std::map<std::string, std::vector<std::pair<std::string, bool>>> map_topic_clients;

		// pentru fiecare client, retine mesajele apartinad topicurilor la care clientul a setat SF=1
		std::map<std::string, std::vector<message_to_TCP>> map_idClient_messages;

		// asocieaza pentru fiecare socket o pereche (ip client TCP, port client TCP)
		// util cand trebuie sa afisam ce conexiunii s-au realizat
		std::map<int, std::pair<std::string, int>> map_socket_IP_Port_Client;

	public:
		Server(int port = 8080) {
			fdmax = 0;
			opt = 1;

			memset((char *) &server_addr, 0, sizeof(struct sockaddr_in));
			server_addr.sin_family = AF_INET;
		    	server_addr.sin_addr.s_addr = INADDR_ANY;
		    	server_addr.sin_port = htons(port);
		}

		~Server() {
			status = close(sockUDP);
			DIE(status < 0, "close socket UDP");

			status = close(sockTCP);
			DIE(status < 0, "close socket TCP");
		}

		// creeam socketii
		// "bind-uim" socketii creati
		// adugam socketii la multimea de descriptori de citire
		void init() {
			FD_ZERO(&read_fds);
			FD_ZERO(&tmp_fds);
			FD_SET(STDIN_FILENO, &read_fds);

			// creez socket-ul UDP
			sockUDP = socket(AF_INET, SOCK_DGRAM, 0);
			DIE(sockUDP < 0, "socket UDP");

		    // legam socket-ul UDP
		    status = bind(sockUDP, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
			DIE(status < 0, "bind UDP error");

			// adaug socket-ul UDP la multimea de socket-uri
			FD_SET(sockUDP, &read_fds);
			if (sockUDP > fdmax) {
				fdmax = sockUDP;
			}

			// creez socket-ul TCP
			sockTCP = socket(AF_INET, SOCK_STREAM, 0);
			DIE(sockTCP < 0, "socket TCP");

			// setam optiunile socket-ului TCP
			status = setsockopt(sockTCP, IPPROTO_TCP, TCP_NODELAY, (char*) &opt, sizeof(int));
		   	DIE(status < 0, "TCP socket options");

			// legam socket-ul TCP
			status = bind(sockTCP, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
			DIE(status < 0, "bind TCP error");

			// punem server-ul sa astepte conexiuni de la clienti TCP
			status = listen(sockTCP, MAX_CLIENTS);
			DIE(status < 0, "listen TCP error");

			// adaug socket-ul TCP la multimea de socket-uri
			FD_SET(sockTCP, &read_fds);
			if (sockTCP > fdmax) {
				fdmax = sockTCP;
			}
		}

		void run() {
			int newsockfd;

			while (1) {
				tmp_fds = read_fds; 
				
				status = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
				DIE(status < 0, "select");

				for (int i = 0; i <= fdmax; i++) {
					if (FD_ISSET(i, &tmp_fds)) {
						// citim de la tastatura
						if (i == STDIN_FILENO) {
							char command[SIZE_COMMAND];
							fgets(command, sizeof(command), stdin);
							command[strlen(command) - 1] = '\0';

							if (strcmp(command, "exit") == 0) {
								close_server();
								return;
							}
							
							continue;
						}

						// a sosit o noua cerere de conexiune de la un client TCP
						if (i == sockTCP) {
							addrlen = sizeof(struct sockaddr);
							memset(&client_addr, 0, sizeof(struct sockaddr_in));
							newsockfd = accept(sockTCP, (struct sockaddr *) &client_addr, &addrlen);
							DIE(newsockfd < 0, "accept");

							// setam optiunile socket-ului TCP nou creat
							status = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &opt, sizeof(int));
		    					DIE(status < 0, "TCP socket options");

							FD_SET(newsockfd, &read_fds);
							if (newsockfd > fdmax) { 
								fdmax = newsockfd;
							}

							std::string IP = inet_ntoa(client_addr.sin_addr);
							int PORT = ntohs(client_addr.sin_port);
							map_socket_idClient.insert({newsockfd, "null"});
							map_socket_IP_Port_Client.insert({newsockfd, std::pair<std::string, int>(IP, PORT)});

							continue;
						}

						// a sosit o datagrama de la un client UDP
						if (i == sockUDP) {
							addrlen = sizeof(struct sockaddr);
							memset(&client_addr, 0, sizeof(struct sockaddr_in));

							UDP_message msg_UDP;
							memset(&msg_UDP, 0, sizeof(UDP_message));

							status = recvfrom(sockUDP, &msg_UDP, sizeof(UDP_message), 0, (struct sockaddr *) &client_addr, &addrlen);
							DIE(status < 0, "recv UDP message error");

							notify_TCP_clients(msg_UDP, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

						    	continue;
						}

						TCP_message msgTCP;
					    	memset(&msgTCP, 0, sizeof(TCP_message));

					    	status = recv(i, &msgTCP, sizeof(TCP_message), 0);
					    	DIE(status < 0, "recv TCP message error");
						
						if (status == 0) {
							std::cout << "Client " << map_socket_idClient.at(i) << " disconnected.\n";
							status = close(i);
							DIE(status < 0, "socket close error");
							// scoatem din multimea de citire socketul inchis 
							FD_CLR(i, &read_fds);
							
							map_idClient_socket.erase(map_socket_idClient.at(i));
							map_clients_alive[map_socket_idClient.at(i)] = false;
							map_socket_idClient.erase(i);
							
							continue;
						}

						// am primit id-ul clientului de TCP
						if (map_socket_idClient.at(i) == "null") {
							bool ok = false;
							map_idClient_socket.insert({msgTCP.var.client_id, i});
							map_socket_idClient[i] = msgTCP.var.client_id;
							if (!map_clients_alive.count(msgTCP.var.client_id)) {
								// client nou
								map_clients_alive.insert({msgTCP.var.client_id, true});
							} else {
								// clientul a mai fost conectat in trecut
								ok = true;
								map_clients_alive[msgTCP.var.client_id] = true;
							}
							
							std::pair<std::string, int> pair = map_socket_IP_Port_Client.at(i);
							map_socket_IP_Port_Client.erase(i);
							std::cout << "New client " << msgTCP.var.client_id << " connected from " << pair.first << ":" << pair.second << ".\n";
							
							if (ok) {
								send_lose_messages(msgTCP.var.client_id, i);
							}
						} else {
							char topic_help[MAX_SIZE_TOPIC + 1];
							memcpy(topic_help, msgTCP.topic, MAX_SIZE_TOPIC);
							topic_help[MAX_SIZE_TOPIC] = '\0';
							if (strcmp(msgTCP.type, "subscribe") == 0) {
								subscribe_client(map_socket_idClient.at(i), topic_help, msgTCP.var.SF);
							} else {
								if (strcmp(msgTCP.type, "unsubscribe") == 0) {
									unsubscribe_client(map_socket_idClient.at(i), topic_help);
								}
							}
						}
					}
				}
			}
		}

	private:
		// notificam toti clientii TCP ca serverul se va deconecta
		void close_server() {
			message_to_TCP msg_TCP;
			memset(&msg_TCP, 0, sizeof(message_to_TCP));
			msg_TCP.serv_state = true;

			for (int i = 0; i <= fdmax; ++i) {
				if (FD_ISSET(i, &read_fds)) {
					if (i != STDIN_FILENO && i != sockUDP && i != sockTCP) {
						status = send(i, &msg_TCP, sizeof(message_to_TCP), 0);
						DIE(status < 0, "error send TCP message");

						status = close(i);
						DIE(status < 0, "socket close error");
						// scoatem din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
					}
				}
			}
		}

		// trimit mesajele neprimite (pt topicurile cu SF setat) deoarece era deconectat
		void send_lose_messages(std::string client_id, int socket) {
			std::vector<message_to_TCP> messages;
			std::vector<message_to_TCP>::iterator help_it;

			if (map_idClient_messages.count(client_id)) {
				messages = map_idClient_messages.at(client_id);

				for (help_it = messages.begin(); help_it != messages.end(); ++help_it) {
					status = send(socket, &(*help_it), sizeof(message_to_TCP), 0);
					DIE(status < 0, "send message to TCP error");
				}
				// sterg intrarea din map(id_client, lista de mesaje)
				map_idClient_messages.erase(client_id);
			}
		}

		// trimitem catre toti abonatii topicului (din msg_UDP) mesajul primit
		void notify_TCP_clients(const UDP_message & msg_UDP, const char *IP, const int port) {
			// pt topic obtin lista de clienti
			// pt fiecare client obtin daca socketul este activ => trimit catre ei
													//	  inactiv => salvez mesajul in lista respectivului
													//               client, daca acesta a setata SF=1
													//				 pentru topicul primit
			
			if (!DATA_TYPE.count((int)msg_UDP.data_type_value)) {
				return;
			}

			char topic_help[MAX_SIZE_TOPIC + 1];
			memcpy(topic_help, msg_UDP.topic, MAX_SIZE_TOPIC);
			topic_help[MAX_SIZE_TOPIC] = '\0';

			if (!map_topic_clients.count(topic_help)) {
				map_topic_clients.insert({topic_help, std::vector<std::pair<std::string, bool>>()});
				return;
			}

			int socket = -1;
			message_to_TCP msg_TCP;
			memset(&msg_TCP, 0, sizeof(message_to_TCP));
			msg_TCP.serv_state = false; // server-ul este inca activ
			memcpy(msg_TCP.topic, msg_UDP.topic, MAX_SIZE_TOPIC);
			strcpy(msg_TCP.data_type, DATA_TYPE.at((int)msg_UDP.data_type_value).c_str());
			memcpy(msg_TCP.content, msg_UDP.content, MAX_SIZE);
			strcpy(msg_TCP.IP, IP);
			msg_TCP.port = port;

			std::vector<std::pair<std::string, bool>> list;
			list = map_topic_clients.at(topic_help);
			std::vector<std::pair<std::string, bool>>::iterator it;
			std::vector<message_to_TCP>::iterator help_it;
			std::vector<message_to_TCP> messages;

			for(it = list.begin(); it != list.end(); ++it) {
				/*
				cazuri
				1. clientul este activ => trimit mesajul
				2. clientul nu este activ, dar are SF setat, adaug mesajul in lista respectiva
				3. clientul nu este activ, si nici nu are setat SF => ignor mesajul in cazul clientului
				*/
				socket = -1;
				if (map_clients_alive.at(it->first)) {
					socket = map_idClient_socket.at(it->first);
				}

				if (socket != -1) {
					// trimi mesajul catre client
					status = send(socket, &msg_TCP, sizeof(message_to_TCP), 0);
					DIE(status < 0, "send message to TCP error");
				} else {
					if (it->second) {
						// adaug pentru clientul respectiv topicul ca sa stie ca trebuie sa il primeasca
						if (map_idClient_messages.count(it->first)) {
							messages = map_idClient_messages.at(it->first);
							messages.push_back(msg_TCP);
							map_idClient_messages[it->first] = messages;
						} else {
							map_idClient_messages.insert({it->first, {msg_TCP}});
						}
					}
				}
			}
		}

		void subscribe_client(std::string client_id, const char *topic, bool SF) {
			std::vector<std::pair<std::string, bool>> list;
			std::vector<std::pair<std::string, bool>>::iterator it;
			if (map_topic_clients.count(topic)) {
				list = map_topic_clients.at(topic);
				// daca clientul este deja abonat la topicul respevctiv, atunci ignor cererea
				for (it = list.begin(); it != list.end(); ++it) {
					if (it->first == client_id) {
						return;
					}
				}
				list.push_back(std::pair<std::string, bool>(client_id, SF));
				map_topic_clients[topic] = list;
			} else {
				map_topic_clients.insert({topic, {std::pair<std::string, bool>(client_id, SF)}});
			}
		}

		void unsubscribe_client(std::string client_id, const char *topic) {
			std::vector<std::pair<std::string, bool>> list;
			std::vector<std::pair<std::string, bool>>::iterator it;
			if (map_topic_clients.count(topic)) {
				list = map_topic_clients.at(topic);
				for (it = list.begin(); it != list.end(); ++it) {
					if (it->first == client_id) {
						list.erase(it);
						break;
					}
				}
				map_topic_clients[topic] = list;
			}
		}
};

int main(int argc, char const *argv[]) {
	DIE(argc != 2, "incorrect number of arguments");
	
	int port = atoi(argv[1]);
	DIE(port == 0, "wrong port value");

	Server *server = new Server(port);
	server->init();
	server->run();

	delete server;
	return 0;
}
