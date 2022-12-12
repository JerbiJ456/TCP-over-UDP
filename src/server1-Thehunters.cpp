#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <climits>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <pthread.h>
#include <vector>
#include <list>
#include <chrono>


#define MTU 1500

using namespace std;
using namespace std::chrono;

int windowSize = 23;

void clearBuf(char* b) {
    int i;
    for (i = 0; i < MTU; i++)
        b[i] = '\0';
}

void bufToSend(char* nSeg, char data[], int n) {
    char toReturn[MTU];
    for (int i=0; i < 6; i++)
        toReturn[i] = nSeg[i];
    for (int i=6; i < n; i++)
        toReturn[i] = data[i-6];
    memcpy(data,toReturn,MTU);
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
    clearBuf(dataBuffer);
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

    int nSent = 0;
    int lastAck = 0;
    int ackIgnore = 0;
    int retransmit = 0;
    bool change = false;
    bool start = true;
    double coeffRTT = 1/windowSize;
    double rttMoy = 0.05;
    
    auto startTime = high_resolution_clock::now();
    struct timeval timeout;      
    timeout.tv_sec = 0;
    timeout.tv_usec = 9000;
    setsockopt (socketData, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout);
    
    while(true) {
        //cout << "Window Size : " << windowSize << endl;
        if (lastAck == nPackets) break;
        if (start) {
            //start = false;
            ackIgnore = 0;
            int lastToBeSent = nPackets > lastAck+windowSize ? lastAck+windowSize : nPackets;
            //cout << "Last to be sent : " << lastToBeSent << " Last ack : " << lastAck << endl;
            for (int i=lastAck+1; i <= lastToBeSent; ++i) {
                //cout << "In LOOP : " << i << endl;
                clearBuf(dataBuffer);
                char bufSeg[6];
                sprintf(bufSeg, "%06d", i);
                memcpy(dataBuffer,bufSeg,6);
                fseek(fp, (MTU-6)*(i-1), SEEK_SET);
                int n = fread(dataBuffer+6, 1, MTU-6, fp);
                sendto(socketData, dataBuffer, n+6, 0, (struct sockaddr *)&addrData, sizeof(addrData));
            }
            nSent = lastToBeSent;
        }
        int receivedAcks = 0;
        int previousWindow = windowSize;
        while (lastAck < nSent && receivedAcks < previousWindow) {
            //cout << "Waiting for ACKS" << endl;
            clearBuf(dataBuffer);
            int n = recvfrom(socketData, dataBuffer, MTU, 0, (struct sockaddr *)&addrData, &sizeData);
            if (n==-1) {
                //cout << "Fin TIMEOUT" << endl;
                start = true;
                retransmit += windowSize-receivedAcks;
                //windowSize -= windowSize > 2 ? 2 : 1;
                break;
            } else if (strncmp(dataBuffer, "ACK", 3) == 0) {
                ////cout << "Ack Reçu" << endl;
                ++receivedAcks;
                //cout << "RACKS : " << receivedAcks << " WindowSize : " << previousWindow << endl;
                char nAck[10];
                memcpy(nAck, dataBuffer+3, n);
                int receivedAck;
                sscanf(nAck, "%d", &receivedAck);
                //cout << "Ack Reçu : " << receivedAck << endl;

                if (lastAck < receivedAck) {
                    //cout << "NEW ACK" << endl;
                    //change = true;
                    lastAck = receivedAck;
                    //windowSize *= lastAck == nSent ? 2 : 1;
                }
            }
        }
        
        
    }
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - startTime);
    sendto(socketData, "FIN", 3, 0, (struct sockaddr *)&addrData, sizeof(addrData));
    cout << "Fichier Envoyé Avec ce nombre de retransmissions : " << retransmit << endl;
    double timeTaken = (double)((double)((int)(duration.count()/1000))+(double)(duration.count()%1000)/1000.0);
    cout << "Cela a pris : " << timeTaken << "s" << endl;
    cout << "Vous avez un débit de : " << ((double)(MTU*nPackets)/(1024.0*1024.0))/timeTaken << " Avec un fichier de cette taille : " << ((double)(filelen)/(1024.0*1024.0)) << endl;
    //cout << dataBuffer << endl;
    close(socketData);
}

int main(int argc, char const *argv[]) {
    if (argc == 1) {
        cout << "Vous n'avez pas donné le bon nombre d'aguments\n" 
        << "Utilisation ./server1 nport tailleFenetre" << endl;
    }

    int portUDP = stoi(argv[1]);
    if (argc > 2) {
        windowSize = atoi(argv[2]);
    }

    char bufferUDP[MTU];
    int nClientPort = 1000;

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
        clearBuf(bufferUDP);
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

                clearBuf(bufferUDP);
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
