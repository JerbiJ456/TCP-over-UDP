all: server1

server1: server1.o
	g++ -o ../bin/server1 server1.o

server1.o: server1.cpp
	g++ -c server1.cpp -o server1.o

clean:
	rm -f ../bin/server1 *.o