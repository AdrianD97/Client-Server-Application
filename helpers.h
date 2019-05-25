#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <map>

/*
 * Macro de verificare a erorilor
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define MAX_CLIENTS 128		// numarul maxim de clienti in asteptare

#define COMM_MAX_SIZE	100  // lungimea maxima a unei comenzii

#define MAX_SIZE	1500 // numarul maxim de octeti din continutul unui mesaj

#define	MAX_SIZE_TOPIC	50	// numarul maxim de octeti din continutul unui topic

#define SIZE 	11 // dimensiunea maxima (in octeti) al unui id si al unui data_type

#define SIZE_COMMAND	20 // lungimea unei comenzi (subscribe/unsubscribe/exit)

#define SIZE_IP 	16 // dimensiunea(in octeti) aunei adrese IPv4

// un map care asocieaza unui integer un string reprezentand tipul continutului proriu-zis din mesaj.
const std::map<unsigned int, std::string> DATA_TYPE = {
	{0, "INT"},
	{1, "SHORT_REAL"},
	{2, "FLOAT"},
	{3, "STRING"}
};

// folosita pentru a primi mesaje de la un client UDP
struct UDP_message {
	char topic[MAX_SIZE_TOPIC]; // topic-ul
	unsigned char data_type_value; // valorea prin care se identifica tipul continutului
	char content[MAX_SIZE]; // continutul propriu-zis al mesajului
}__attribute__((packed));

// catre server pot trimite doua tipuri de mesaje
//	1. un mesaj in care ii trimit id-ul clientului a carei cerere de conectare
//	tocmai a fost acceptata de server.
//	2. un mesaj in care instiintez serverul daca un client se (dez)aboneaza
//	(de) la un topic
// in acest fel este mai rentabil sa utilizez un union

union help {
	char client_id[SIZE]; // id-ul clientului
	bool SF; // specifica dorinta clientului de primi sau nu mesajele care au fost publiccate
			// in perioada in care el a fost inactiv
};

// folosita pentru a primi mesaje de la clienti TCP
struct TCP_message {
	char type[SIZE_COMMAND]; // tipul mesajului
	char topic[MAX_SIZE_TOPIC]; // topicul
	help var; // id-ul clientului(daca trimitem un mesaj imediat dupa acceptarea cererii de conexiune)
			  // SF, daca trimitem un mesaj in care clientul isi exprima o dorinta
}__attribute__((packed));

// folosita pentru a trimite mesaje catre un client TCP de la server
struct message_to_TCP {
	bool serv_state; // utilizat pt a diferentia mesajul de incheiere a conexiunii fata de mesajele obisnuite
	char topic[MAX_SIZE_TOPIC]; // topicul
	char data_type[SIZE]; // tipul continutului
	char content[MAX_SIZE]; // continutul propriu-zis al mesajului
	char IP[SIZE_IP]; // IP-ul clientului de UDP
	int port; // portul clientului de UDP
}__attribute__((packed));

#endif
