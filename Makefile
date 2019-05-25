FLAGS = -std=c++11 -Wall

build:
	g++ $(FLAGS) server.cpp -o server
	g++ $(FLAGS) subscriber.cpp -o subscriber

clean:
	rm server subscriber
