#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <climits>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <ctime>
#include <sys/types.h> 
#include <sys/time.h>


#define MTU 1500

using namespace std;
using namespace std::chrono;

typedef struct __attribute__((__packed__)) {
    int seqNum;
    int n;
    char data[MTU];
    struct timeval sent;
}chunk;

unsigned int windowSize = 1;
unsigned int ssthreash = 20;
unsigned int maxWindowSize = 50;
unsigned int startingWindow = 1;
int lastAck = 0;

//chrono::_V2::system_clock::time_point start;

vector<chunk> window;

mutex ack_mut;

bool startThread = false;
bool fastRetransmit = false;
bool timeup = false;

void recvThread(int SocketAck, struct sockaddr_in addrData) {

    char ackRCV[20];
    int rt;
    socklen_t sizeData = sizeof(addrData);
	struct timeval timeout;
    //chrono::_V2::system_clock::time_point stop;
	int oldacks = 0;
	timeout.tv_sec = 0;
	timeout.tv_usec = 5000;
    unsigned int newTimeout;

    while (!startThread) {
        continue;
    }

    bool first = true;

    while (startThread) {

        if (first) {
            if (setsockopt(SocketAck, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
                return;
            }
            first = false;
        } /*else {
            // (1 - alpha) * RTT + alpha * (time_ack_received - time_segment_sent)
            timeout.tv_sec = 0;
            newTimeout = (long)(1-0.2)*timeout.tv_usec + (long)0.2*duration_cast<microseconds>(stop - start).count();
            timeout.tv_usec = newTimeout;
            cout << timeout.tv_usec << endl;
            if (setsockopt(SocketAck, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0) {
                return;
            }
        }*/

        int n = recvfrom(SocketAck, ackRCV, 20, 0, (struct sockaddr *)&addrData, &sizeData);
        if (n < 0) {
            ack_mut.lock();
            timeup = true;
            ack_mut.unlock();
        }
        else if(n > 0) {
            char nAck[20];
            memcpy(nAck, ackRCV+3, n);
            int receivedAck;
            sscanf(nAck, "%d", &receivedAck);
            ack_mut.lock();
            if(receivedAck > lastAck) {
                window.erase(window.begin(), window.begin()+(receivedAck-lastAck));
                lastAck = receivedAck;
                windowSize = min(maxWindowSize, windowSize+2);
                //stop = high_resolution_clock::now();
            } else if (receivedAck == lastAck) {
                if(++oldacks == 3){
					oldacks = 0;
					fastRetransmit = true;
                }
            }
            ack_mut.unlock();
        }
    }
	return;
}


void processClient(int socketData, int nClientPort) {
    char dataBuffer[MTU];
    struct sockaddr_in addrData;
    socketData = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketData == -1) {
        cout << "Création Socket UDP non Réussie" << endl;
        exit(0);
    }
    cout << "Socket UDP crée avec succes" << endl;
    int optData = 1;
    setsockopt(socketData, SOL_SOCKET, SO_REUSEADDR, (const void *)&optData, sizeof(int));
    memset((char *)&addrData, 0, sizeof(addrData));
    socklen_t sizeData = sizeof(addrData);
    addrData.sin_family = AF_INET;
    addrData.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addrData.sin_port = htons(nClientPort);
    
    if ((bind(socketData, (struct sockaddr *)&addrData, sizeof(addrData))) != 0) {
        cout << "Bind non réussi" << endl;
        exit(0);
    }
    cout << "Bind Réussi" << endl;

    memset((char *)&addrData, 0, sizeof(addrData));
    recvfrom(socketData, dataBuffer, MTU, 0, (struct sockaddr *)&addrData, &sizeData);
    FILE *fp;
    fp = fopen(dataBuffer, "rb");
    if (!fp) {
        cout << "Erreur lors de la lecture du fichier pour le client sur le port " << nClientPort << endl;
        close(socketData);
        exit(1);
    }
    cout << "Fichier Ouvert" << endl;
    fseek(fp, 0, SEEK_END); 
    long int filelen = ftell(fp);
    rewind(fp);      
    int nPackets = (filelen / (MTU - 6)) + 1;  

    cout << "File length : " << filelen << " Nombre Paquets : " << nPackets << endl;

    window.clear();

    int nSent = 0;
    int ackIgnore = 0;
    int retransmit = 0;
    
    auto startTime = high_resolution_clock::now();

    chunk dataChunk = {};
    bool make_thread = true;
	thread th0;
    th0 = thread(recvThread, socketData, addrData);
    
    while(true) {
        if (lastAck == nPackets) break;
        //start = high_resolution_clock::now();
        while ((nSent < nPackets) && (window.size() < windowSize)) {
            memset(&dataChunk, 0, sizeof(chunk));
            nSent++;
            dataChunk.seqNum = nSent;
            char bufSeg[6];
            sprintf(bufSeg, "%06d", nSent);
            memcpy(dataChunk.data,bufSeg,6);
            fseek(fp, (MTU-6)*(nSent-1), SEEK_SET);
            int n = fread(dataChunk.data+6, 1, MTU-6, fp);
            dataChunk.n = n+6;
            sendto(socketData, dataChunk.data, n+6, 0, (struct sockaddr *)&addrData, sizeof(addrData));
            //seqToIndex.insert(pair<int, int>(nSent, window.size()));
            ack_mut.lock();
            window.push_back(dataChunk);
            ack_mut.unlock();
        }
        if (!startThread)
            startThread = true;
        ack_mut.lock();
		if(timeup) {
			//cout<<"TIME OUT ATTEINT"<<endl;
			//sendto(socketData, window[0].data, window[0].n, 0, (struct sockaddr *)&addrData, sizeof(addrData));
			for(auto pac :  window) {
				sendto(socketData, pac.data, pac.n, 0, (struct sockaddr *)&addrData, sizeof(addrData));
			}
            cout << windowSize << endl;
            windowSize /= 2+1;
			timeup = false;
            ackIgnore++;
		}
        else if (fastRetransmit) {
            //sendto(socketData, window[0].data, window[0].n, 0, (struct sockaddr *)&addrData, sizeof(addrData));
            for(auto pac :  window) {
				sendto(socketData, pac.data, pac.n, 0, (struct sockaddr *)&addrData, sizeof(addrData));
			}
            fastRetransmit = false;
            retransmit++;
        }
		ack_mut.unlock();
    }
    startThread = false;
    th0.join();

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - startTime);
    sendto(socketData, "FIN", 3, 0, (struct sockaddr *)&addrData, sizeof(addrData));
    cout << "---------------------------------------------------------------------------------------" << endl;
    cout << "Client au port n : " << nClientPort << endl;
    cout << "Fichier Envoyé Avec ce nombre de retransmissions : " << retransmit << " et ce nombre de timeouts : " << ackIgnore << endl;
    double timeTaken = (double)((double)((int)(duration.count()/1000))+(double)(duration.count()%1000)/1000.0);
    cout << "Cela a pris : " << timeTaken << "s" << endl;
    cout << "Vous avez un débit de : " << ((double)(MTU*nPackets)/(1024.0*1024.0))/timeTaken << " Avec un fichier de cette taille : " << ((double)(filelen)/(1024.0*1024.0)) << endl;
    cout << "---------------------------------------------------------------------------------------" << endl;
    //cout << dataBuffer << endl;
    close(socketData);
}

int main(int argc, char const *argv[]) {
    if (argc == 1) {
        cout << "Vous n'avez pas donné le bon nombre d'aguments\n" 
        << "Utilisation ./server1 nport" << endl;
    }
    int portUDP = stoi(argv[1]);

    char bufferUDP[MTU];
    int nClientPort = 1500;

    struct sockaddr_in addrUDP, addrData;

    int socketUDP = socket(AF_INET, SOCK_DGRAM, 0);

    if (socketUDP == -1)
    {
        cout << "Création Socket UDP non Réussie" << endl;
        exit(0);
    }

    cout << "Socket UDP crée avec succes" << endl;

    int optUDP = 1;
    setsockopt(socketUDP, SOL_SOCKET, SO_REUSEADDR, (const void *)&optUDP, sizeof(int));

    memset((char *)&addrUDP, 0, sizeof(addrUDP));
    addrUDP.sin_family = AF_INET;
    addrUDP.sin_addr.s_addr = htonl(INADDR_ANY);
    addrUDP.sin_port = htons(portUDP);

    if ((bind(socketUDP, (struct sockaddr *)&addrUDP, sizeof(addrUDP))) != 0)
    {
        cout << "Bind non réussi" << endl;
        exit(0);
    }

    cout << "Bind Réussi" << endl;

    int socketData;
    struct sockaddr_in addrClient;

    while(true) {
        //clearBuf(bufferUDP);
        memset((char *)&addrClient, 0, sizeof(addrClient));
        socklen_t sizeClient = sizeof(addrClient);
        bool handShake = false;
        while(!handShake) {
            cout << "En attente du SYN" << endl;
            int n = recvfrom(socketUDP, bufferUDP, MTU, 0, (struct sockaddr *)&addrClient, &sizeClient);
            if (strncmp("SYN", bufferUDP, 3) == 0) {
                cout << "SYN reçu" << endl;
                if(nClientPort == portUDP) ++nClientPort;
                cout << "Envoie du SYN-ACK" << nClientPort << endl;
                string synAck = "SYN-ACK" + to_string(nClientPort);

                //clearBuf(bufferUDP);
                if (fork() == 0) {
                    close(socketUDP);
                    processClient(socketData, nClientPort);
                    exit(0);
                }
                sendto(socketUDP, synAck.c_str(), MTU, 0, (struct sockaddr *)&addrClient, sizeof(addrClient));
                int n = recvfrom(socketUDP, bufferUDP, MTU, 0, (struct sockaddr *)&addrClient, &sizeClient);
                if (strncmp("ACK", bufferUDP, 3) == 0) {
                    cout << "ACK reçu" << endl;
                    handShake = true;
                }
            }
        }
        nClientPort++;
        cout << "Connexion établie !!" << endl;
    }


    return 0;
}
