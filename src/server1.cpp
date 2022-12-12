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


#define MTU 1500

struct chuncks {
   char* array;
};

using namespace std;

int windowSize = 50;

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
    cout << "Fichier Ouvert" << endl;
    if (!fp) {
        cout << "Erreur lors de la lecture du fichier pour le client sur le port " << nClientPort << endl;
        close(socketData);
        exit(1);
    }
    fseek(fp, 0, SEEK_END); 
    long int filelen = ftell(fp);
    rewind(fp);      
    int nPackets = (filelen / (MTU - 6)) + 1;  
    cout << " Nombre Paquets : " << nPackets << "In file : " << endl;  

    char allSegments[nPackets+1][MTU];
    cout << " Nombre Paquets : " << nPackets << endl; 
    int nLast;

    for (int i=1; i<=nPackets; ++i) {
        //cout << "inside for" << endl;
        char bufSeg[6];
        clearBuf(dataBuffer);
        sprintf(bufSeg, "%06d", i);
        memcpy(dataBuffer,bufSeg,6);
        int n = fread(dataBuffer+6, 1, MTU-6, fp);
        char copy[n];
        memcpy(copy,dataBuffer+6,n);
        if (i == nPackets) nLast = n;
        memcpy(allSegments[i], dataBuffer, MTU);
        //cout << "N: " << n << " " << strncmp(copy,allSegments[i]+6,n) << endl;
    }
    fclose(fp);
    cout << "File length : " << filelen << " Nombre Paquets : " << nPackets << " Last n : " << nLast << endl;

    FILE *toWrite;

    toWrite = fopen("copy.jpg", "wb");

    for(int i=1; i<=nPackets; ++i) {
        int n = i == nPackets ? nLast : MTU-6;
        fwrite(allSegments[i]+6,1,n,toWrite);
    }

    int nSent = 0;
    int lastAck = 0;
    int ackIgnore = 0;
    int retransmit = 0;
    bool change = false;
    bool start = true;
    double coeffRTT = 1/windowSize;
    double rttMoy = 0.05;
    
    
    
    while(true) {
        //cout << "Window Size : " << windowSize << endl;
        struct timespec timeout;
        timeout.tv_sec = (long)(coeffRTT*rttMoy*1e-6);
        timeout.tv_nsec = (long)(coeffRTT*rttMoy * 1e3) % (long)(1e9);
        setsockopt (socketData, SOL_SOCKET, SO_RCVTIMEO, &timeout,sizeof timeout);
        if (lastAck == nPackets) break;
        if (start) {
            start = false;
            ackIgnore = 0;
            int lastToBeSent = nPackets > lastAck+windowSize ? lastAck+windowSize : nPackets;
            //cout << "Last to be sent : " << lastToBeSent << " Last ack : " << lastAck << endl;
            for (int i=lastAck+1; i <= lastToBeSent; ++i) {
                //cout << "In LOOP : " << i << endl;
                int n = i == nPackets ? nLast+6 : MTU;
                sendto(socketData, allSegments[i], n, 0, (struct sockaddr *)&addrData, sizeof(addrData));
            }
            nSent = lastToBeSent;
        } else if (change) {
            //cout << "Inside Change" << endl;
            change = false;
            ackIgnore = 0;
            windowSize += 2;
            int lastToBeSent = nPackets > lastAck+1+windowSize ? lastAck+1+windowSize : nPackets;
            for (int i=lastAck+1; i <= lastToBeSent; ++i) {
                int n = i == nPackets ? nLast+6 : MTU;
                sendto(socketData, allSegments[i], n, 0, (struct sockaddr *)&addrData, sizeof(addrData));
            }
            nSent = lastToBeSent;
        }
        clearBuf(dataBuffer);
        int n = recvfrom(socketData, dataBuffer, MTU, 0, (struct sockaddr *)&addrData, &sizeData);
        if (n==-1) {
            //cout << "Fin TIMEOUT" << endl;
            start = true;
            retransmit++;
            windowSize -= 2;
        } else if (strncmp(dataBuffer, "ACK", 3) == 0) {
            //cout << "Ack Reçu" << endl;
            char nAck[10];
            memcpy(nAck, dataBuffer+3, n);
            int receivedAck;
            sscanf(nAck, "%d", &receivedAck);
            //cout << "Ack Reçu : " << receivedAck << endl;

            if (lastAck < receivedAck) {
                change = true;
                lastAck = receivedAck;
            }
        }
        
        
    }
    sendto(socketData, "FIN", 3, 0, (struct sockaddr *)&addrData, sizeof(addrData));
    cout << "Fichier Envoyé" << endl;
    //cout << dataBuffer << endl;
    close(socketData);
}

int main(int argc, char const *argv[])
{
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
