/*
 * client11b.c - Interactive UDP Client for Lab 1.1
 * Prompts user for input, sends to server, measures round trip time
 * Follows the Lab11-RFC protocol specification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#define SERVERPORT "10010"
#define MAXBUFLEN 1038

// Protocol message structure
#pragma pack(1)
typedef struct {
    uint16_t length;      // Total message length in network byte order
    uint32_t seq_num;     // Sequence number in network byte order
    uint64_t timestamp;   // Timestamp in network byte order
    char message[1024];   // Variable length string
} protocol_msg_t;
#pragma pack()

// Function to get current time in milliseconds since epoch
uint64_t get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    char buf[MAXBUFLEN];
    char input[1024];
    protocol_msg_t msg;
    protocol_msg_t recv_msg;
    uint64_t send_time, recv_time;
    uint32_t seq_num = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s hostname\n", argv[1]);
        exit(1);
    }

    // Set up address structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Create socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to create socket\n");
        return 2;
    }

    // Main client loop
    while(1) {
        printf("Enter a string to send (or 'quit' to exit): ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove newline character
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "quit") == 0) {
            break;
        }

        // Prepare protocol message
        memset(&msg, 0, sizeof(msg));
        strncpy(msg.message, input, sizeof(msg.message) - 1);
        msg.message[sizeof(msg.message) - 1] = '\0';
        
        uint16_t msg_len = strlen(msg.message);
        uint16_t total_len = 2 + 4 + 8 + msg_len;
        
        msg.length = htons(total_len);
        msg.seq_num = htonl(seq_num);
        send_time = get_timestamp_ms();
        msg.timestamp = htobe64(send_time);  // Convert to big-endian (network order)

        // Send the message
        if ((numbytes = sendto(sockfd, &msg, total_len, 0,
                p->ai_addr, p->ai_addrlen)) == -1) {
            perror("client: sendto");
            continue;
        }

        printf("client: sent %d bytes to %s\n", numbytes, argv[1]);

        // Receive the echo
        if ((numbytes = recvfrom(sockfd, &recv_msg, MAXBUFLEN, 0, NULL, NULL)) == -1) {
            perror("client: recvfrom");
            continue;
        }

        recv_time = get_timestamp_ms();

        // Extract and display the response
        uint16_t recv_len = ntohs(recv_msg.length);
        uint32_t recv_seq = ntohl(recv_msg.seq_num);
        uint64_t recv_timestamp = be64toh(recv_msg.timestamp);

        printf("client: received echo: '%s'\n", recv_msg.message);
        printf("client: sequence number: %u\n", recv_seq);
        printf("client: round trip time: %lu ms\n", recv_time - send_time);
        printf("---\n");

        seq_num++;
    }

    freeaddrinfo(servinfo);
    close(sockfd);
    return 0;
}