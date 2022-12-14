all: serveur-Thehunters

serveur-Thehunters: allObjectFiles
	g++ serveur1-Thehunters.o -o ../bin/serveur1-Thehunters
	g++ serveur2-Thehunters.o -o ../bin/serveur2-Thehunters
	g++ serveur3-Thehunters.o -o ../bin/serveur3-Thehunters
	rm -f *.o

allObjectFiles: serveur1-Thehunters.cpp serveur2-Thehunters.cpp serveur3-Thehunters.cpp 
	g++ -c serveur1-Thehunters.cpp -o serveur1-Thehunters.o 
	g++ -c serveur2-Thehunters.cpp -o serveur2-Thehunters.o
	g++ -c serveur3-Thehunters.cpp -o serveur3-Thehunters.o

clean:
	rm -f ../bin/serveur1-Thehunters ../bin/serveur2-Thehunters ../bin/serveur3-Thehunters