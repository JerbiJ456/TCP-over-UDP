all: server1-Thehunters server2-Thehunters server3-Thehunters

server1-Thehunters: server1-Thehunters.o
	g++ -o ../bin/server1-Thehunters server1-Thehunters.o

server1-Thehunters.o: server1-Thehunters.cpp
	g++ -c server1-Thehunters.cpp -o server1-Thehunters.o

server2-Thehunters: server2-Thehunters.o
	g++ -o ../bin/server2-Thehunters server2-Thehunters.o

server2-Thehunters.o: server2-Thehunters.cpp
	g++ -c server2-Thehunters.cpp -o server2-Thehunters.o

server3-Thehunters: server3-Thehunters.o
	g++ -o ../bin/server3-Thehunters server3-Thehunters.o

server3-Thehunters.o: server3-Thehunters.cpp
	g++ -c server3-Thehunters.cpp -o server3-Thehunters.o

clean:
	rm -f ../bin/server1-Thehunters ../bin/server2-Thehunters ../bin/server3-Thehunters *.o