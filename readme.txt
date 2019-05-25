		Tema 2 PC - Aplicatie client-server TCP si UDP pentru gestionarea mesajelor

	Structura tema:
		server.cpp
		subscriber.cpp
		helpers.h

	Fisierul server.cpp cuprinde implementarea celei mai imporatante componente din aplicatia noastra: server-ul, componenta care se ocupa de gestionarea mesajelor primite de la clientii(TCP si UDP).
	Entitatea server este reprezentata de clasa Server. Clasa contine niste campuri esentiale, care
	practic ajuta la implementarea logicii de gestionare a mesajelor. Aceste campuri sunt:
		- map_clients_alive -> un map care specifica pentru fiecare client(care s-a conectat cel putin o
		data la server) daca este activ sau nu la un moment de timp. Acesta este util in momentul in
		care sosete un mesaj UDP, si dorim sa-l trimitem catre clientii TCP activi.
		- map_idClient_socket -> un map care asocieaza pentru fiecare client TCP(identificat prin id) un
		socket. Acesta este util cand trimitem mesaje catre clienti TCP, deoarece in momentul in care
		primim un mesaj UDP, avem o lista de clienti(vom detalia mai jos) abonati la un anumit topic,
		si trimitem catre cei activi mesajul. Insa pentru a trimite un mesaj avem nevoie de o "priza",
		astfel map-ul are rolul de a ne furniza socket-ul pe care comunica server-ul cu un anumit client.
		- map_socket_idClient -> un map care asocieaza fiecarui socket un client TCP(identificat prin id).
		Acesta este util in momentul in care primim mesaje de la clientii TCP. Utilitatea lui consta in
		faptul ca in momentul in care sosete un mesaj TCP, pe un anumit socket, eu pot sa identific
		clientul de la care a sosit mesajul curent.
		- map_topic_clients -> un map care asocieaza pentru fiecare topic o lista de perechi de forma
		(id_client, SF), unde id_client = id-ul clientului care s-a abonat la topicul respectiv, iar
		SF=poate avea una dintre valorile 0(clientul nu este interesat de mesajele care sunt publicate
		cand el este inactiv) sau 1(clientul doreste sa primeasca mesajele publicate in perioada in care
		a fost inactiv). Utilitatea acestei structuri consta in faptul ca in momentul in care soseste un
		mesaj UDP, putem obtine foarte rapid lista de clienti abonati la topicul asociat mesajului sosit.
		- map_idClient_messages -> un map care asocieaza pentru fiecare client(identificat prin id) o
		lista de mesaje sosite pe peioada in care el a fost inactiv, mesaje apartinand topic-urilor
		pentru care clientul nu doreste sa piarda nici-un mesaj publicat. Acesta este util in momentul
		in care un client se reconecteaza si acesta trebuie sa primeasca mesajele care au fost publicate
		in timpul in care el a fost inactiv.
		- map_socket_IP_Port_Client -> un map care asocieaza fiecarui socket creat o pereche de forma
		(ip_client, port_client). Acesta este utilizat in momentul in care dorim sa afisam informatii
		despre o noua conexiune, in sensul ca acesta retine ip-ul clientului TCP si portul acestuia,
		pana in momentul in care clientul ii trimite server-ului id-ul prin care este identificat.

	Functionare server:
		1. initializare:
			- metoda init(...) realizeaza initializarea server-ului, mai exact aceasta se ocupa de:
				* crearea socket-ilor (UDP is TCP).
				* configurarea acestora(bind, setsockopt, listen).
				* adugarea socket-ilor creati la multimea de citire.
		2. rulare:
			- metoda run(...) simuleaza comportamentul server-ului.
			Server-ul primeste diferite mesaje/comenzi(sosite pe diferiti socketi). Pentru fiacare
			mesaj/comanda primit/a server-ul trebuie sa realizeze o actiune. Actiunile realizate de
			server sunt:
				1. incheire conexiuni. Daca primeste de la tastatura comanda 'exit', acesta notifica
				printr-un mesaj clientii TCP activi in legatura cu inchiderea conexiunii, utilizand
				metoda close_server(...). In cazul in care se primeste o comanda invalida, acesta o
				ignora.
				2. acceptarea de conexiuni. In momentul in care sosete o cerere de conexiune de la un
				client TCP, acesta asocieaza initial socket-ului creat string-ul "null". Acest lucru ma
				ajuta sa diferentiez un mesaj care contine id-ul clientului de un mesaj care contine
				efectiv o dorinta a clientului(subscribe/unsubscribe). De asemenea, asocieaza pentru
				socket-ul creat o perecehe de forma (ip_client, port_client).
				3. primirea de datagrame de la clientii UDP. In momentul in care soseste un mesaj de la
				un client UDP, server-ul apeleaza metoda notify_TCP_clients(...) care trimite catre
				toti clientii activi(care sunt abonati la topicul asociat mesajului) mesajul primit.
				In cazul in care topicul nu exista, acesta este creat, iar pentru el este asociata o
				lista goala(semnificad faptul ca nu exista inca clienti abonati la topicul respectiv).
				De asemenea, salveaza pentru clientii inactivi, dar care au setat SF=1 pentru topicul
				respectiv, mesajul pentru a-l putea trimite in momentul in care acesta se vor reconecta.
				4. Deconectare clienti TCP. In momentul in care un client TCP se deconecteaza, server-ul
				ii va seta starea respectivului client ca fiind inactiv(map-ul map_clients_alive) si
				de asemenea va sterge intrarea (id_client, socket) din map-ul map_idClient_socket, precum
				si intrarea(socket, id_client) din map-ul map_socket_idClient, deoarece acestea nu vor
				mai fi valide la o reconectare ulterioara a clientului(se va creea un nou socket in
				momentul reconectarii).
				5. primire mesaje de la clientii TCP. Server-ul poate primi doua tipuri de mesaje de la un client TCP, si anume:
					1. un mesaj(pe care il primeste imediat dupa ce a acceptat cererea de conexiune) in
					care acesta primeste id-ul clientului caruia tocmai ce i-a acceptat cererea de
					conexiune. In cazul in care clientul a mai fost conectat, imediat dupa primirea
					mesajului, server-ul ii va transmite toate mesajele(daca exista) asociate
					topic-urilor pentru care el a setat SF=1.Pentru transmiterea acestor mesaje
					apeleaza metoda send_lose_messages(...).
					2. un mesaj in care clientul isi exprima o anumita dorinta(mai exact doreste sa se aboneze/dezaboneze (de) la un topic). 
						Daca mesajul primit reflecta dorinta clientului de a se abona la un anumit topic,
						server-ul apeleaza metoda subscribe_client(...). Metoda verifica daca topic-ul
						exista, caz in care aduga respectivul client la lista de abonati pentru topic-ul
						respectiv(numai in cazul in care acesta nu s-a abonat deja). In schimb daca topicul
						nu exista, se creeaza o intrarea (in map-ul map_topic_clients) cu noul topic la
						care este abonat initial doar clientul care a initiat cererea de abonare.
						In schimb daca mesajul primit reflecta dorinta clientului de a se dezabona de
						la un anumit topic, server-ul apeleaza metoda unsubscribe_client(...) care va
						sterge clientul din lista de abonati asociata topicului respectiv.
						Daca mesajul reflecta altceva decat ce a fost descris mai sus, atunci server-ul
						ignora mesajul.

	Fisiserul subscriber.cpp cuprinde implementarea clientului TCP. Entitatea subscriber este reprezentata de clasa Subscriber.
	Functionare subscriber:
		1. initializare
			- metoda init(...) se ocupa de:
				* crearea socket-ului pe care clientul va comunica cu server-ul
				* trimiterea cerererii de conectare la server
				* trimiterea unui mesaj care contine id-ul sau, imediat dupa acceptarea cerererii de
				catre server
				* adugarea socket-ului creat la multimea de citire.
		2. rulare subscriber
			- metoda run(...) simuleaza comporatmentul unui client TCP.
			Clientul poate primi:
			- o comanda de la tastatura. Comanda poate reprezenta dorinta clientului de a se (dez)abona
			(de) la un anumit topic sau o comanda de incheire a activitatii sale. In momentul primirii
			unei comenzii, aceasta este parsata utilizand metoda parse(...). Dupa aceasta operatie, se identifica tipul comenzii((un)subacribe/exit). Daca comanda reflecta o dorinta a
			clientului((un)subscribe) se trimite un mesaj catre server prin care specifica dorinta acestuia.
			In cazul in care se primeste comanda 'exit', clientul se deconecteaza(devine inactiv).
			Daca acesta primeste o comanda invalida, aceasta va fi ignorata.
			- un mesaj de la server. Daca de la server primeste un mesaj care are campul serv_state setat 
			la true, atunci clientul este notificat ca server-ul isi va incheia activitatea, deci si clientul 
			isi va termina activitatea. In schimb, daca clientul primeste un mesaj in care serv_state este 
			setat la false, atunci acesta apeleaza metoda decode_message(...) care va decodifica mesajul 
			primit conform regulilor din enuntul temei.

	Fisierul helpers.h contine constantele si structurile utilizate pentru reprezentarea fiecarui
	tip de mesaj utilizat.

	Obs: As dori sa felicit pe responasabilii temei. Sincer, este cel mai "misto" lucru/proiect/tema...
	pe care l-am facut de cand sunt la facultate, din toate punctele de vedere (idee, implementare, etc).