#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#define SERVERPORT "10010"
#define MAXBUFLEN 1100

long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec * 1000000 + tv.tv_usec);
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    char input[1000];
    char send_buf[MAXBUFLEN];
    char recv_buf[MAXBUFLEN];
    unsigned short msg_len;
    unsigned int seq_num = 1;
    long long timestamp;
    long long send_time, recv_time;

    if (argc != 2) {
        fprintf(stderr,"usage: client11b hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to create socket\n");
        return 2;
    }

    // main client loop
    while(1) {
        printf("Enter string to send (or 'quit' to exit): ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "quit") == 0) {
            break;
        }
        
        int string_len = strlen(input);
        int total_len = 2 + 4 + 8 + string_len;
        
        unsigned short net_msg_len = htons(total_len);
        unsigned int net_seq_num = htonl(seq_num);
        long long net_timestamp = get_time_ms();
        
        memcpy(send_buf, &net_msg_len, 2);
        memcpy(send_buf + 2, &net_seq_num, 4);
        memcpy(send_buf + 6, &net_timestamp, 8);
        memcpy(send_buf + 14, input, string_len);

        send_time = get_time_ms();

        if ((numbytes = sendto(sockfd, send_buf, total_len, 0,
                 p->ai_addr, p->ai_addrlen)) == -1) {
            perror("sendto");
            continue;
        }

        printf("sent %d bytes\n", numbytes);

        if ((numbytes = recvfrom(sockfd, recv_buf, MAXBUFLEN, 0, NULL, NULL)) == -1) {
            perror("recvfrom");
            continue;
        }

        recv_time = get_time_ms();

        printf("received echo: %.*s\n", numbytes - 14, recv_buf + 14);
        printf("round trip time: %.2f ms\n", (double)recv_time / 1000.0 - (double)send_time / 1000.0);
        printf("---\n");

        seq_num++;  
    }

    freeaddrinfo(servinfo);
    close(sockfd);
    return 0;
}