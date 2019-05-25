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
#include <iomanip>

#include "helpers.h"

class Subscriber {
	private:
		std::string id;
		std::string serv_IP;
		int serv_port;
		
		int sockfd;
		socklen_t addrlen;
    	struct sockaddr_in server_addr;
    	int opt;
    	int status;

    	fd_set read_fds;
		fd_set tmp_fds;
		int fdmax;
	
	public:
		Subscriber(std::string id, std::string IP, int port) {
			this->id = id;
			serv_IP = IP;
			serv_port = port;
			fdmax = 0;
			opt = 1;
		}

		~Subscriber() {
			status = close(sockfd);
			DIE(status < 0, "close socket");
		}

		void init() {
			// completare informatii despre adresa serverului
			memset((char *) &server_addr, 0, sizeof(struct sockaddr_in));
			server_addr.sin_family = AF_INET;
		    server_addr.sin_addr.s_addr = inet_addr(serv_IP.c_str());
		    server_addr.sin_port = htons(serv_port);

		    FD_ZERO(&read_fds);
		    FD_ZERO(&tmp_fds);
		    FD_SET(STDIN_FILENO, &read_fds);

		    // creeam socket-ul
		    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
		    DIE(sockfd < 0, "create socket error");

		    // setam optiunile socket-ului TCP
			status = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &opt, sizeof(int));
		    DIE(status < 0, "TCP socket options");

		    // ne conectam la server
		    status = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
		    DIE(status < 0, "connection fails");

		    // adaugam socket-ul la multimea de citire
		    FD_SET(sockfd, &read_fds);
		    if (sockfd > fdmax) {
		        fdmax = sockfd;
		    }

		    // trimitem catre server un mesaj cu id-ul clientului curent
		    TCP_message msg;
		    memset(&msg, 0, sizeof(TCP_message));
		    strcpy(msg.var.client_id, this->id.c_str());

		    status = send(sockfd, &msg, sizeof(TCP_message), 0);
		    DIE(status < 0, "send message fails");
		}

		void run() {
			message_to_TCP msgServ;
			TCP_message msg;
			char command[COMM_MAX_SIZE];
			unsigned int help;
			std::vector<std::string> list;
			int len;

			while (1) {
		        tmp_fds = read_fds;
		        
		        status = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		        DIE(status < 0, "select");

		        for (int i = 0; i <= fdmax; i++) {
		            if (FD_ISSET(i, &tmp_fds)) {
		                if (i == sockfd) {
		                    memset(&msgServ, 0, sizeof(message_to_TCP));
		                    status = recv(sockfd, &msgServ, sizeof(message_to_TCP), 0);
		                    DIE(status < 0, "recv message fails");

		                    if (msgServ.serv_state) {
		                        std::cout << "Conexiune incheiata\n";
		                        return;
		                    }

		                    decode_message(msgServ);
		                } else {
		                	list.clear();
		                    fgets(command, COMM_MAX_SIZE, stdin);
		                    parse(command, list);

		                    if (list.size() >= 2) {
		                    	if (list[0] != "subscribe" && list[0] != "unsubscribe") {
			                    	continue;
			                    }

			                    memset(&msg, 0, sizeof(TCP_message));
			                    strcpy(msg.type, list[0].c_str());
			                    
			                    len = strlen(list[1].c_str());
			                    if (len == MAX_SIZE_TOPIC) {
			                    	memcpy(&msg.topic, list[1].c_str(), MAX_SIZE_TOPIC);
			                    } else {
			                    	if (len < MAX_SIZE_TOPIC) {
			                    		strcpy(msg.topic, list[1].c_str());
			                    	} else {
			                    		continue;
			                    	}
			                    }
			                 	
			                    if (list[0] == "subscribe") {
			                    	if (list.size() > 2) {
				                    	help = atoi(list[2].c_str());
				                    	if (help != 0 && help != 1) {
				                    		continue;
				                    	}
				                    	msg.var.SF = help;
				                    } else {
				                    	continue;
				                    }
			                    }

			                    status = send(sockfd, &msg, sizeof(TCP_message), 0);
			                    DIE(status < 0, "send message fails");

			                    std::cout << list[0] << " " << list[1] << '\n';
		                    } else {
		                    	if (list.size() == 1 && list[0] == "exit") {
		                    		std::cout << "Conexiune incheiata\n";
		                    		return;
		                    	}
		                    }
		                }
		            }
		        }
		    }
		}

	private:
		/*
		parseaza comanda primita de la tastatura
		*/
		void parse(char *command, std::vector<std::string> &list) {
			char *p = strtok(command, " \n");
			if (p != NULL) {
				list.push_back(p); // (un)subscribe
				p = strtok(NULL, " \n");
				if (p != NULL) {
					list.push_back(p); // topic
					p = strtok(NULL, " \n");
					if (p != NULL) {
						list.push_back(p); // (SF)
					}
				}
			}
		}

		/*
			decodifica mesajul primit de la server
		*/
		void decode_message(message_to_TCP &msg) {
			int help;
			double var;
			uint32_t value1;
			uint16_t value2;
			int sign;
			char topic_help[MAX_SIZE_TOPIC + 1];

			memcpy(topic_help, msg.topic, MAX_SIZE_TOPIC);
		    topic_help[MAX_SIZE_TOPIC] = '\0';

		    std::cout << msg.IP << ":" << msg.port << " - ";
            std::cout << topic_help << " - " << msg.data_type;

			if (strcmp(msg.data_type, "INT") == 0) {
				memset(&value1, 0, sizeof(uint32_t));
				memcpy(&value1, msg.content + 1, 4);
				sign = msg.content[0] ? -1 : 1;
				std::cout << " - " << (int)ntohl(value1) * sign << '\n';

				return;
			}

			if (strcmp(msg.data_type, "SHORT_REAL") == 0) {
				memset(&value2, 0, sizeof(uint16_t));
				memcpy(&value2, msg.content, 2);

				help = (int)ntohs(value2);
				var = help / 100.0;

				std::cout << std::fixed;
				std::cout << " - " << std::setprecision(2) << var << '\n';

				return;
			}

			if (strcmp(msg.data_type, "FLOAT") == 0) {
				memset(&value1, 0, sizeof(uint32_t));
				memcpy(&value1, msg.content + 1, 4);
				sign = msg.content[0] ? -1 : 1;
				unsigned int expo = 0;
				memcpy(&expo, msg.content + 5, 1);

				help = (int)ntohl(value1);
				var = (help * 1.0) / (pow(10, expo) * 1.0);

				var *= sign;
				std::cout << " - " << std::fixed;
				std::cout << std::setprecision(expo) << var << '\n';

				return;
			}

			char content_help[MAX_SIZE];
			memcpy(content_help, msg.content, MAX_SIZE);
			content_help[MAX_SIZE] = '\0';
			std::cout << " - " << content_help << '\n';
		}

		/*
		calculeaza n la purea expo.
		*/
		double pow(int n, int expo) {
			if (expo == 0) {
				return 1;
			}

			double res = n;
			for ( ; expo > 1; --expo) {
				res *= n;
			}

			return res;
		}
};

int main(int argc, char const *argv[]) {
	DIE(argc != 4, "incorrect number of arguments");

	int port = atoi(argv[3]);
	DIE(port == 0, "wrong port value");

	Subscriber *subscriber = new Subscriber(argv[1], argv[2], port);

	subscriber->init();
	subscriber->run();

	delete subscriber;

	return 0;
}