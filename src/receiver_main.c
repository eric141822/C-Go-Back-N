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
#include <sys/time.h>
#define MAX_BUFFER_SIZE 4096
#define MAX_TIMEOUT 30
#define MAX_RESEND 200
#define MAX_SEQ_NUM 256

struct sockaddr_in si_me, si_other;
int s, slen; 

struct packet_t {
    int seq_num;
    int length;
    char data[MAX_BUFFER_SIZE];
};


void diep(char *s) {
    perror(s);
    exit(1);
}

int addSeqNum (int seq, int size) {
    int new_seq = (seq + size) % MAX_SEQ_NUM;
    return new_seq;
}

int substractSeqNum (int seq, int size) {
    int new_seq = (seq - size + MAX_SEQ_NUM) % MAX_SEQ_NUM;
    return new_seq;
}


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);
    FILE* fp;
    fp = fopen(destinationFile, "wb");
    if (fp == NULL) {
        diep("Could not open file to write to.");
    }
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */    
    ssize_t recv_bytes, sent_bytes;
    struct packet_t packet;
    int ack_num;
    int expected_num = 0;
    struct timeval timeout = {MAX_TIMEOUT, 0};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof (timeout));
    while (1) {
        recv_bytes = recvfrom(s, &packet, MAX_BUFFER_SIZE, 0, (struct sockaddr*) &si_other, (socklen_t *) &slen);
        if (recv_bytes == -1) {
            // Did not receive data for 30 SECONDS.
            printf("Timeout, breaking loop.\n");
            break;
        }
        if (packet.seq_num == -1) {
            // Received -1, sender is done sending.
            printf("Sender finished sending, breaking loop.\n");
            ack_num = -1;
            sent_bytes = sendto(s, &ack_num, sizeof (ack_num), 0, (struct sockaddr*) &si_other, slen);
            break;
        }

        if (packet.seq_num == expected_num) {
            packet.data[recv_bytes] = '\0'; // null terminate the data.
            fwrite(packet.data, sizeof (char), packet.length, fp);

            ack_num = packet.seq_num;
            sent_bytes = sendto(s, &ack_num, sizeof (ack_num), 0, (struct sockaddr*) &si_other, slen);
            if (sent_bytes == -1) {
                diep("sendto() failed");
            }

            // increment expected_num
            expected_num = addSeqNum(expected_num, 1);
        } else {
            // Not expected seq num, ACK with expected_seq - 1;
            ack_num = substractSeqNum(expected_num, 1);
            sent_bytes = sendto(s, &ack_num, sizeof (ack_num), 0, (struct sockaddr*) &si_other, slen);
            if (sent_bytes == -1) {
                diep("sendto() failed");
            }
        }
    }
    printf("Closing socket\n");
    close(s);
	printf("%s received.\n", destinationFile);
    printf("Closing fptr\n");
    fclose(fp);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "missing arguments, usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}