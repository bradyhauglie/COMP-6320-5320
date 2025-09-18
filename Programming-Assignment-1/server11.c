/*
 * server11.c - UDP Echo Server for Lab 1.1
 * Listens on port 10010 and echoes back received messages
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

#define PORT "10010"
#define MAXBUFLEN 1038  // 2 + 4 + 8 + 1024 max message size

// Protocol message structure
#pragma pack(1)
typedef struct {
    uint16_t length;      // Total message length in network byte order
    uint32_t seq_num;     // Sequence number in network byte order
    uint64_t timestamp;   // Timestamp in network byte order
    char message[1024];   // Variable length string (up to 1024 bytes)
} protocol_msg_t;
#pragma pack()

int main(void)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    int rv;
    char buf[MAXBUFLEN];
    int numbytes;

    // Set up address structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      // Use IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket
    hints.ai_flags = AI_PASSIVE;    // Use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Create and bind socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("UDP Echo Server: waiting for connections on port %s...\n", PORT);

    // Main server loop
    while(1) {
        addr_len = sizeof their_addr;
        
        // Receive message from client
        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1, 0,
                (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            continue;
        }

        printf("server: received %d bytes from client\n", numbytes);

        // Echo the message back to the sender
        if (sendto(sockfd, buf, numbytes, 0,
                (struct sockaddr *)&their_addr, addr_len) == -1) {
            perror("sendto");
            continue;
        }

        printf("server: echoed message back to client\n");
    }

    close(sockfd);
    return 0;
}