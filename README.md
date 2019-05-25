		 Client-Server application for managing messages

	The application has three main components:
		1. server:
			- connects customers in the platform with the purpose of publishing and subscribing to 
			messages.
		    - he is the mediation component.
		    - he can only get the exit command from the keyboard. The exit command closes the 
		    server and all clients are disconnected.
		   - he is implemented in C/C++ .

		2. TCP clients:
			- a TCP client connects to server, can receive different keyboard commands, such:
				* subscribe: announces the server that the client is interested in a specific topic
				* unsubscribe: announces the server that a client is no longer interested in a 
				specific topic.
				* exit: client disconnects
			- he is implemented in C/C++.

		3. UDP clients:
			- an UDP client publishs messages in the platform. The messages are send to 
			the server.
			- he is implemented in python

	The application include a STORE&FORWARD component of the sent messages when the clients are 
	disconnected.
	TCP clients and UDP clients can be in any number.


	Application run:
		1. make
		2. run server: 
			 ./server <SERVER_PORT>
		3. run a TCP client:
			 ./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>
		   run an UDP client:
			 python3 udp_client.py <SERVER_IP> <SERVER_PORT>
			 or
			 python3 udp_client.py --mode manual <SERVER_IP> <SERVER_PORT>
			 or
			 python3 udp_client.py --source-port 1234 --input_file three_topics_payloads.json 
			 --mode random --delay 2000 <SERVER_IP> <SERVER_PORT>
