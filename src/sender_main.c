#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#define MAX_BUFFER_SIZE 3000
#define MAX_TIMEOUT 5
#define MAX_RESEND 500
#define MAX_SEQ_NUM 256
#define WINDOW_SIZE 16

#define max(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a > _b ? _a : _b;      \
    })

struct sockaddr_in si_other;
int s, slen;

struct packet_t
{
    int seq_num;
    int length;
    char data[MAX_BUFFER_SIZE];
};

void diep(char *s)
{
    perror(s);
    exit(1);
}

int isACKBetween(int ack, int lower, int upper)
{
    if (ack < lower)
    {
        ack += MAX_SEQ_NUM;
    } // wrap around;
    return (ack >= lower && ack <= upper);
}

int addSeqNum(int seq, int size)
{
    int new_seq = (seq + size) % MAX_SEQ_NUM;
    return new_seq;
}

int substractSeqNum(int seq, int size)
{
    int new_seq = (seq - size + MAX_SEQ_NUM) % MAX_SEQ_NUM;
    return new_seq;
}

void reliablyTransfer(char *hostname, unsigned short int hostUDPport, char *filename, unsigned long long int bytesToTransfer)
{
    // Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        printf("Could not open file to send.");
        exit(1);
    }

    /* Determine how many bytes to transfer */

    slen = sizeof(si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *)&si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0)
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Send data and receive acknowledgements on s*/
    struct packet_t packet;
    struct packet_t packets[WINDOW_SIZE];
    ssize_t read_bytes, sent_bytes, recv_bytes;
    int seq_num = 0;
    int ack_num = 0;
    struct timeval timeout = {0, MAX_TIMEOUT * 1000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    while (!feof(fp) && bytesToTransfer > 0)
    {
        int resend_count = 0;
        int send_base = seq_num; // send_base for the current window.
        int cummulated_ack = send_base;
        for (int i = 0; i < WINDOW_SIZE; i++)
        {
            packet.seq_num = seq_num;
            int send_size = (bytesToTransfer < MAX_BUFFER_SIZE) ? bytesToTransfer : MAX_BUFFER_SIZE;
            read_bytes = fread(packet.data, 1, send_size, fp);

            bytesToTransfer -= send_size;

            packet.length = read_bytes;
            packets[i] = packet;

            sent_bytes = sendto(s, &packets[i], sizeof(packets[i]), 0, (struct sockaddr *)&si_other, slen);
            if (sent_bytes == -1)
                diep("sendto() failed");

            // clear data buffer;
            bzero(packet.data, MAX_BUFFER_SIZE);

            seq_num = addSeqNum(seq_num, 1); // add 1 to seq_num;
        }

        // wait for ACKs

        while (1)
        {
            recv_bytes = recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen);
            if (recv_bytes == -1)
            {
                // timeout, resend all packets from cummulated_ack;
                for (int i = cummulated_ack; i < WINDOW_SIZE + cummulated_ack; i++)
                {
                    int index = i % WINDOW_SIZE; // convert index to 0-31;
                    sent_bytes = sendto(s, &packets[index], sizeof(packets[index]), 0, (struct sockaddr *)&si_other, slen);
                    if (sent_bytes == -1)
                        diep("sendto() failed");
                }
            }
            else
            {
                // recevied ACK
                // if not isACKbetween(S, S+N-1), ignore;
                if (isACKBetween(ack_num, send_base, send_base + WINDOW_SIZE - 1) != 1)
                {
                    // printf("Received ack for packet %d, out-of-range.\n", ack_num);
                }
                // ACK is in the current window, but not the last ACK.
                else if (isACKBetween(ack_num, send_base, send_base + WINDOW_SIZE - 2) == 1)
                {
                    // update cumulated ack;
                    cummulated_ack = max(substractSeqNum(ack_num, send_base) + 1, cummulated_ack);
                }
                // if isACK(S+N+1), last ACK, break;
                else if (isACKBetween(ack_num, send_base + WINDOW_SIZE - 1, send_base + WINDOW_SIZE - 1) == 1)
                {
                    break;
                }
            }
        }
    }
    // send packet with -1 to inform sending done.
    packet.seq_num = -1;
    int resend_count = 0;
    sendto(s, &packet, sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
    if (sent_bytes == -1)
        diep("sendto() failed");
    recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen);
    while (ack_num != -1)
    {
        sent_bytes = sendto(s, &packet, sizeof(packet), 0, (struct sockaddr *)&si_other, slen);
        if (sent_bytes == -1)
            diep("sendto() failed");
        recvfrom(s, &ack_num, sizeof(ack_num), 0, (struct sockaddr *)&si_other, (socklen_t *)&slen);
        resend_count++;

        if (resend_count == MAX_RESEND)
        {
            printf("Max retry reached. Closing the socket\n");
            close(s);
            exit(1);
        }
    }
    printf("Closing the socket\n");
    close(s);
    printf("Closing file ptr.\n");
    fclose(fp);
    return;
}

/*
 *
 */
int main(int argc, char **argv)
{

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5)
    {
        fprintf(stderr, "missing arguments, usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int)atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return (EXIT_SUCCESS);
}