// PRS Projet Alex DEVAUCHELLE Yasser ISAAM
// 5 Mb/s mono client, client 1 30 Mb/s, client 2 10 MB/s

// 1 350 0.4 1.5 0 2 1024 -> 19s
// 1 350 0.4 1.5 1 2 1024 -> 18s
// 1 350 0.4 1.5 1 1 1024 -> 18s

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>

#define UDP_BUFF_SIZE 1500
#define UDP_ACK_SIZE 6
#define SA struct sockaddr

// #define DEBUG_OUTPUT 0
// #define DEBUG_OUTPUT1 0
#define DEBUG_OUTPUT_START_END 0
// #define DEBUG_OUTPUT_FIN 0
// #define DEBUG_OUTPUT_FFRTS 0
// #define DEBUG_OUTPUT_ACK 0
// #define DEBUG_STATS 0

// EDITABLE VARIABLES

double default_RTT = 10000;
int slowstar_windowsize = 10;
int Swindow_size_after_timeout = 1;
double coeff_estimated_RTT = 0.875; // 0.875
int coeff_sstresh = 1.5;            // useless if slowstart_afterSSTRESH = 1
int slowstart_afterSSTRESH = 1;
int ffrts_max = 1; // x + 1 = nb of ack before ffrts start

int array_SIZE = 1024;

// OTHER VARIABLES

struct sockaddr_in cli_addr;
FILE *fileptr;

char *DATA_BUFFER;
int *len_DATA_BUFFER;

int last_SEG_BUFFERED = 0;

int last_ACK = 0;
int last_SND = 0;
int timeout = 0;
int max_window = 1;
int ssthresh = 0;
int slowStart = 1;
int ffrts = 0;
int ffrts_ACK = 0;

double timeout_RTT = 10000;
double estimated_RTT = 0;
double dev_RTT = 0;

struct timeval timeout_RTT_time;

int nb_seg;

struct thread_args
{
    int sockfdDATA;
    long *pointerArray[]; // first half of the array is the sequence number, second half is the time
};

struct timeout_args
{
    int ackID;
    int sockfdDATA;
    char *bufferDATA;
};

void *timeoutThread(void *param)
{

    printf("Thread %ld STARTED\n", pthread_self());


    struct timeout_args *args = (struct timeout_args *)param;
    struct timespec timespec_RTT;
    timespec_RTT.tv_nsec = (long)(timeout_RTT * 1e3) % (long)(1e9);
    timespec_RTT.tv_sec = (long)(timeout_RTT * 1e-6);

    // printf("timespec_RTT.tv_nsec = %ld ; timespec_RTT.tv_sec = %ld\n", timespec_RTT.tv_nsec, timespec_RTT.tv_sec);
    printf("Timer of %ld ns + %ld s started on thread %ld\n", (long)(timeout_RTT * 1e3) % (long)(1e9), (long)(timeout_RTT * 1e-6), pthread_self());


    nanosleep(&timespec_RTT, NULL);

    if (args->ackID > last_ACK)
    {

        printf("TO THREAD %ld FOR SEG %d & LAST_ACK = %d\n", pthread_self(), args->ackID, last_ACK);

        timeout = 1;
    }


    printf("END THREAD %ld\n", pthread_self());


    pthread_exit(0);
}

void *ackThread(void *param)
{
    struct thread_args *args = (struct thread_args *)param;
    char UDP_buff[UDP_BUFF_SIZE];
    struct sockaddr_in cli_addr_ack;
    memset((char *)&cli_addr_ack, 0, sizeof(cli_addr_ack));
    int len = sizeof(cli_addr_ack);

    while (1) // last_ACK < nb_seg
    {

        bzero(UDP_buff, sizeof(UDP_buff));
        printf("Waiting for ACK ...\n");

        recvfrom(args->sockfdDATA, UDP_buff, sizeof(UDP_buff), 0, (struct sockaddr *)&cli_addr_ack, &len);

        char ack[32];
        memcpy(ack, UDP_buff + 3, UDP_ACK_SIZE);
        int idAck = atoi(ack);
        printf("ACK %d rcv\n", idAck);


        if (idAck > last_ACK)
        {
            printf("LAST_ACK update from %d to %d\n", last_ACK, idAck);

            // RTT
            struct timeval now;
            gettimeofday(&now, NULL);
            //-printf("Time now  : %ld\n", (now.tv_sec / 1000) + (now.tv_usec * 1000));
            //-printf("Time sent : %ld\n", *args->pointerArray[idAck + nb_seg - 1]);

            // int var = time in micro sec from now

            int diff_rtt = ((now.tv_sec * 1000000) + (now.tv_usec) - *args->pointerArray[(idAck - 1) % array_SIZE]);
            // printf("diff_rtt : %d\n", diff_rtt);
            estimated_RTT = coeff_estimated_RTT * estimated_RTT + (1 - coeff_estimated_RTT) * diff_rtt;
            // printf("Estimated RTT : %f\n", estimated_RTT);
            dev_RTT = 0.75 * dev_RTT + 0.25 * abs(diff_rtt - estimated_RTT);
            //-printf("Dev RTT : %f\n", dev_RTT);
            timeout_RTT = estimated_RTT + 4 * dev_RTT;
            //-printf("Timeout RTT : %f\n", timeout_RTT);
            timeout_RTT = timeout_RTT / 5;

            // Slow Start
            slowStart = (max_window < ssthresh) ? 1 : slowstart_afterSSTRESH;
            max_window = (slowStart) ? (max_window + idAck - last_ACK) : (max_window + (idAck - last_ACK) / max_window);

            max_window = (max_window > array_SIZE) ? array_SIZE : max_window;

            last_ACK = idAck;

            ffrts = 0;
        }
        else if (idAck == last_ACK)
        { // DUPLICATED ACK
            printf("Dupicate ACK %d rcv\n", idAck);

            ffrts++;
            if (ffrts == ffrts_max)
            {
                printf("FFRTS ACK %d (BEFORE) max_window = %d, ssthresh = %d\n", idAck + 1, max_window, ssthresh);

                ssthresh = max_window;
                max_window = (int)((max_window + 1) * 0.75); // +1 to avoid 0
                slowStart = 1;
                // slowStart = 0;   FAST RECOVERY
                ffrts_ACK = idAck;
                printf("FFRTS ACK %d, max_window = %d, ssthresh = %d\n", idAck + 1, max_window, ssthresh);

            }
        }
    }
}

//______________________________________________________________________________________________________________________________________________________________________________________________________
//======================================================================================================================================================================================================
//______________________________________________________________________________________________________________________________________________________________________________________________________

// Function designed for chat between client and server.
void func(int sockfdDATA)
{

    char UDP_buff[UDP_BUFF_SIZE];
    memset((char *)&cli_addr, 0, sizeof(cli_addr));
    int n;
    int connection = 1;
    int Swindow = max_window;
    while (connection)
    {
        int len = sizeof(cli_addr);
        bzero(UDP_buff, sizeof(UDP_buff));
        int open = 1;
        while (open)
        {
            recvfrom(sockfdDATA, UDP_buff, sizeof(UDP_buff), 0, (struct sockaddr *)&cli_addr, &len);

            printf("FILENAME rcv: %s\n", UDP_buff);
            printf("Transfer START\n");

            // OPEN FILE

            fileptr = fopen(UDP_buff, "rb"); // Open the file in binary mode
            if (fileptr == NULL)
            {

                printf("Cannot open file %s\n", UDP_buff);

            }
            else
            {
                open = 0;
            }
        }

        // GET FILE LENGTH & NB SEG
        fseek(fileptr, 0, SEEK_END);   // Jump to the end of the file
        long filelen = ftell(fileptr); // Get the current byte offset in the file
        rewind(fileptr);               // Jump back to the beginning of the file
        nb_seg = (filelen / (UDP_BUFF_SIZE - UDP_ACK_SIZE)) + 1;


        printf("File length : %ld\n", filelen);
        printf("Nb seg : %d\n", nb_seg);

        size_t len_data;
        char data_buff[UDP_BUFF_SIZE - UDP_ACK_SIZE - 1];
        pthread_t tid[array_SIZE], ackTID;
        long pointerArray[array_SIZE];

        struct thread_args *args = malloc(sizeof(struct thread_args) + array_SIZE * sizeof(long *));
        args->sockfdDATA = sockfdDATA;
        for (int i = 0; i < array_SIZE; i++)
        {
            pointerArray[i] = 0;
            args->pointerArray[i] = &pointerArray[i];
        }

        int err = pthread_create(&ackTID, NULL, ackThread, args);
        while (err != 0)
        {

            printf("ERROR %d : pthread_create failed\n", err);

            pthread_detach(ackTID);

            pthread_cancel(ackTID);
            pthread_join(ackTID, NULL);
            err = pthread_create(&ackTID, NULL, ackThread, args);
        }

        int last_CLEAN = 0;

        DATA_BUFFER = malloc((UDP_BUFF_SIZE)*array_SIZE * sizeof(char));
        bzero(DATA_BUFFER, (UDP_BUFF_SIZE)*array_SIZE * sizeof(char));
        len_DATA_BUFFER = malloc(array_SIZE * sizeof(int));

        char numero_seg[UDP_ACK_SIZE + 1];

        for (int i = 0; ((i < array_SIZE) & (i < nb_seg)); i++)
        {
            bzero(numero_seg, sizeof(numero_seg));
            sprintf(numero_seg, "%06d", i + 1);
            memcpy(DATA_BUFFER + ((UDP_BUFF_SIZE)*i), numero_seg, 6);
            len_DATA_BUFFER[i] = fread(DATA_BUFFER + ((i + 1) * 6) + i * (UDP_BUFF_SIZE - UDP_ACK_SIZE), 1, UDP_BUFF_SIZE - UDP_ACK_SIZE, fileptr);
        }
        last_SEG_BUFFERED = array_SIZE;
        int nb_SEG_SENT = 0;


        while (last_ACK < nb_seg)
        {
            if (timeout)
            {


                printf("TIMEOUT in main adjusting var and cancel all threads\n");


                // KILL other timeout
                for (int i = last_ACK + 1; i <= last_SND; i++)
                    pthread_cancel(tid[(i - 1) % array_SIZE]);

                // SEG adjust
                last_SND = last_ACK;

                // Window adjust
                ssthresh = (int)(max_window / 1.25);
                max_window = slowstar_windowsize;
                slowStart = 1;
                Swindow = max_window;

                // timeout_RTT adjust

                timeout_RTT = default_RTT;

                // Timeout FLAG OFF
                timeout = 0;
            }

            if (ffrts >= ffrts_max)
            {
                printf("FAST 1.1 RETRANSMIT FOR SEG %d\n", ffrts_ACK + 1);

                pthread_cancel(tid[(ffrts_ACK) % array_SIZE]);
                timeout = 0;
                Swindow++;
                printf("FAST 1.2 RETRANSMIT FOR SEG %d, Swindow = %d\n", ffrts_ACK + 1, Swindow);

            }

            while (((last_SEG_BUFFERED - array_SIZE) < ((last_ACK))) & (last_SEG_BUFFERED < nb_seg))
            {

                printf("%d----------------------------------------------------------------------------------------\n", last_SEG_BUFFERED);

                bzero(numero_seg, sizeof(numero_seg));
                bzero(DATA_BUFFER + ((UDP_BUFF_SIZE) * (last_SEG_BUFFERED % array_SIZE)), UDP_BUFF_SIZE);
                sprintf(numero_seg, "%06d", last_SEG_BUFFERED + 1);
                memcpy(DATA_BUFFER + ((UDP_BUFF_SIZE) * (last_SEG_BUFFERED % array_SIZE)), numero_seg, 6);
                len_DATA_BUFFER[(last_SEG_BUFFERED % array_SIZE)] = fread(DATA_BUFFER + (((last_SEG_BUFFERED % array_SIZE) + 1) * 6) + (last_SEG_BUFFERED % array_SIZE) * (UDP_BUFF_SIZE - UDP_ACK_SIZE), 1, UDP_BUFF_SIZE - UDP_ACK_SIZE, fileptr);

                last_SEG_BUFFERED++;


                printf("SEG %d buffered at index %d\n", last_SEG_BUFFERED + 1, last_SEG_BUFFERED % array_SIZE);

            }

            while ((Swindow > 0) & ((last_SND < nb_seg) || (ffrts >= ffrts_max)))
            {
                bzero(UDP_buff, sizeof(UDP_buff));
                bzero(data_buff, sizeof(data_buff));

                if (last_SND < last_ACK)
                {
                    last_SND = last_ACK + 1;
                }

                int recovery_lastSND = last_SND;


                printf("Swindow = %d sending a seg\n", Swindow);

                if (ffrts >= ffrts_max)
                {
                    pthread_cancel(tid[(ffrts_ACK) % array_SIZE]);
                    last_SND = ffrts_ACK + 1;
                    printf("FAST 2 RETRANSMIT FOR SEG %d\n", last_SND);

                }
                else
                {
                    last_SND = last_SND < last_ACK ? last_ACK : last_SND + 1;

                    printf("Normal transmit for SEG %d\n", Swindow);

                }

                // fseek(fileptr, (last_SND - 1) * (UDP_BUFF_SIZE - UDP_ACK_SIZE), SEEK_SET);

                // sprintf(UDP_buff, "%06d", last_SND); // last_SND = id du Segment
                //  len_data = fread(UDP_buff + 6, 1, UDP_BUFF_SIZE - UDP_ACK_SIZE, fileptr); // Read in file
                len_data = len_DATA_BUFFER[((last_SND - 1) % array_SIZE)];

                printf("len_data = %ld\n", len_data);

// memcpy(UDP_buff + 6, DATA_BUFFER + (((last_SND - 1) * (UDP_BUFF_SIZE - UDP_ACK_SIZE + 1)) % array_SIZE), len_data);

// memcpy(UDP_buff + UDP_ACK_SIZE, data_buff, len_data);

                printf("Sending SEG %d with buffer at index %d\n", last_SND, ((last_SND - 1) % array_SIZE));

                sendto(sockfdDATA, DATA_BUFFER + ((((last_SND - 1) % array_SIZE) * (UDP_BUFF_SIZE))), len_data + UDP_ACK_SIZE, 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));

                printf("SEG %d send\n", last_SND);

                struct timeval now;
                gettimeofday(&now, NULL);
                pointerArray[(last_SND - 1) % array_SIZE] = (now.tv_sec * 1000000) + (now.tv_usec);

                struct timeout_args *args = malloc(sizeof(struct timeout_args));
                args->ackID = last_SND;
                int err = pthread_create(&tid[(last_SND - 1) % array_SIZE], NULL, timeoutThread, args);

                while (err != 0)
                {

                    printf("ERROR %d : pthread_create failed FOR SEG %d & THREAD %ld\n", err, last_SND, tid[(last_SND - 1) % array_SIZE]);

                    pthread_detach(tid[(last_SND - 1) % array_SIZE]);
                    pthread_cancel(tid[(last_SND - 1) % array_SIZE]);
                    pthread_join(tid[(last_SND - 1) % array_SIZE], NULL);
                    err = pthread_create(&tid[(last_SND - 1) % array_SIZE], NULL, timeoutThread, args);
                    // if (err != 0)
                    //{
                    //    printf("ERROR %d ON RETRY: pthread_create failed FOR SEG %d & THREAD %ld\n", err, last_SND, tid[(last_SND - 1) % array_SIZE]);
                    //    exit(0);
                    //}
                }


                printf("THREAD %ld START FOR SEG %d , frrts = %d, Swindow = %d\n", tid[(last_SND - 1) % array_SIZE], last_SND, ffrts, Swindow);

                Swindow--;

                if ((ffrts >= ffrts_max) & (last_SND == ffrts_ACK + 1))
                {
                    printf("FAST 3 RETRANSMIT FOR SEG %d DONE\n", last_SND);

                    last_SND = recovery_lastSND;
                    ffrts = 0;

                    printf("LAST SEND %d\n", last_SND);

                }


                nb_SEG_SENT++;

            }

            Swindow = last_ACK - (last_SND - max_window);
            if (Swindow < 0)
                Swindow = 0;
            if (Swindow > max_window)
                Swindow = max_window;


            if (Swindow > 0)
                printf("Last_ACK = %d ; Swindow = %d ; max_window = %d ; last_SND = %d\n", last_ACK, Swindow, max_window, last_SND);

        }


        printf("nb_SEG_SENT = %d\n", nb_SEG_SENT);


        // CLOSE FILE
        fclose(fileptr);

        bzero(UDP_buff, sizeof(UDP_buff));
        sprintf(UDP_buff, "FIN");
        sendto(sockfdDATA, UDP_buff, sizeof(UDP_buff), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
        printf("FIN sent\n");
        printf("Transfer END\n");


        bzero(UDP_buff, sizeof(UDP_buff));
        sprintf(UDP_buff, "FIN");
        for (int i = 1; i < 30; i++)
        {
            sendto(sockfdDATA, UDP_buff, sizeof(UDP_buff), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
            usleep(10 ^ i);
            printf("FIN sent\n");

        }
        bzero(UDP_buff, sizeof(UDP_buff));

        fd_set rset;

        // clear the descriptor set
        FD_ZERO(&rset);

        // get maxfd
        int maxfdp1 = sockfdDATA + 1;
        FD_SET(sockfdDATA, &rset);

        struct timeval tv;
        tv.tv_sec = (long)(2 * timeout_RTT * 1e-6);
        tv.tv_usec = (long)(2 * timeout_RTT);

        int nready = select(maxfdp1, &rset, NULL, NULL, &tv);
        while (nready > 0)
        {
            if (FD_ISSET(sockfdDATA, &rset))
            {
                n = recvfrom(sockfdDATA, UDP_buff, sizeof(UDP_buff), 0, (struct sockaddr *)&cli_addr, &len);
                if (n < 0)
                    printf("error in rcvfrom at line 357\n");
                else
                    printf("MSG rcv after FIN sent, cleaning socket...\n");


                nready = select(maxfdp1, &rset, NULL, NULL, &tv);
            }
        }

        bzero(UDP_buff, sizeof(UDP_buff));
        sprintf(UDP_buff, "FIN");
        for (int i = 1; i < 30; i++)
        {
            sendto(sockfdDATA, UDP_buff, sizeof(UDP_buff), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
            usleep(10 ^ i);
            printf("FIN sent\n");

        }

        connection = 0;
        pthread_cancel(ackTID);
    }

    close(sockfdDATA);
    pthread_exit((void *)0);
}

int main(int argc, const char *argv[])
{
    char UDP_buffUDP[UDP_BUFF_SIZE];
    /* process commmand line */
    if (argc < 3)
    {
        fprintf(stderr, "\nSyntax: %s <port_UDPserveur> [RTT] [slowstart_windowsize] \n\n\n", argv[0]);
        return (0);
    }

    int portUDP = atoi(argv[1]);
    int portDATA;
    // portDATA = atoi(argv[2]);
    portDATA = portUDP + 1;

    // optional parameter
    if (argc > 2)
        default_RTT = atoi(argv[2]);
    timeout_RTT = default_RTT;
    if (argc > 3)
        slowstar_windowsize = atoi(argv[3]);
    ssthresh = coeff_sstresh * slowstar_windowsize;

    // Socket create

    int sockfdUDP, sockfdDATA;

    struct sockaddr_in servaddrUDP, dataaddr;

    // UDP -------------------------------------------------------------------------------------------------------------------------------------------------

    sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfdUDP == -1)
    {

        printf("Error : UDP socket creation failed...\n");

        exit(0);
    }

    printf("Success : UDP Socket created successfully\n");

    int optvalUDP = 1;
    setsockopt(sockfdUDP, SOL_SOCKET, SO_REUSEADDR, (const void *)&optvalUDP, sizeof(int));

    // IP , PORT
    memset((char *)&servaddrUDP, 0, sizeof(servaddrUDP));
    servaddrUDP.sin_family = AF_INET;
    servaddrUDP.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY (0.0.0.0)= all int available , INADDR_LOOPBACK (127.0.0.1)= localhost
    servaddrUDP.sin_port = htons(portUDP);

    // BIND SOCKET UDP & IP/PORT

    if ((bind(sockfdUDP, (SA *)&servaddrUDP, sizeof(servaddrUDP))) != 0)
    {

        printf("Error: UDP Socket bind failed...\n");

        exit(0);
    }

    printf("Success: UDP Socket bind successfully\n");


    struct sockaddr_in cli_addr;

    int connection = 1;
    while (connection)
    {
        int len = sizeof(cli_addr);
        bzero(UDP_buffUDP, sizeof(UDP_buffUDP));
        memset((char *)&cli_addr, 0, sizeof(cli_addr));
        int n = recvfrom(sockfdUDP, UDP_buffUDP, sizeof(UDP_buffUDP), 0, (struct sockaddr *)&cli_addr, &len);
        if (strncmp("SYN", UDP_buffUDP, 3) == 0)
        {
            printf("SYN rcv,sending SYN-ACK%d\n", portDATA);


            portDATA = (portDATA + 1) % 9999;
            portDATA = (portDATA < 1000) ? portDATA + 1000 : portDATA;

            int sockfdDATA;

            sockfdDATA = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfdDATA == -1)
            {
                printf("Error : UDP socket creation failed...\n");

                exit(0);
            }
            printf("Success : UDP Socket created successfully\n");

            int optvalDATA = 1;
            setsockopt(sockfdDATA, SOL_SOCKET, SO_REUSEADDR, (const void *)&optvalDATA, sizeof(int));

            memset((char *)&dataaddr, 0, sizeof(dataaddr));
            dataaddr.sin_family = AF_INET;
            dataaddr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY (0.0.0.0)= all int available , INADDR_LOOPBACK (127.0.0.1)= localhost
            dataaddr.sin_port = htons(portDATA);

            if ((bind(sockfdDATA, (SA *)&dataaddr, sizeof(dataaddr))) != 0)
            {
                printf("Error: DATA Socket bind failed...\n");

                exit(0);
            }
            printf("Success: DATA Socket bind successfully\n");


            sprintf(UDP_buffUDP, "SYN-ACK%d", portDATA);
            sendto(sockfdUDP, UDP_buffUDP, sizeof(UDP_buffUDP), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
            bzero(UDP_buffUDP, sizeof(UDP_buffUDP));
            if (fork() == 0)
            {
                close(sockfdUDP);
                func(sockfdDATA);
                exit(0);
            }
            else
            {
                // while (1)
                ;
            }
        }
        else if (strncmp("ACK", UDP_buffUDP, 3) == 0)
        {
            printf("ACK rcv,connection established\n");

        }
    }

    close(sockfdUDP);
}