#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080

int main(int argc, char *argv[]) {
    if(argc != 4) {
        printf("Usage: %s <operation> <operand1> <operand2>\n", argv[0]);
        printf("Operations: + - x /\n");
        return 1;
    }
    
    int sockfd;
    struct sockaddr_in server_addr;
    char request[9];
    char response[14];
    
    // Parse arguments
    char op = argv[1][0];
    uint32_t a = atoi(argv[2]);
    uint32_t b = atoi(argv[3]);
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    // Connect to server
    connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    // Build request (9 bytes)
    request[0] = op;
    *(uint32_t*)(request + 1) = htonl(a);
    *(uint32_t*)(request + 5) = htonl(b);
    
    // Send request
    send(sockfd, request, 9, 0);
    
    // Receive response
    recv(sockfd, response, 14, 0);
    
    // Parse response
    char resp_op = response[0];
    uint32_t resp_a = ntohl(*(uint32_t*)(response + 1));
    uint32_t resp_b = ntohl(*(uint32_t*)(response + 5));
    uint32_t result = ntohl(*(uint32_t*)(response + 9));
    uint8_t valid = response[13];
    
    // Display result
    printf("Operation: %c\n", resp_op);
    printf("Operands: %u, %u\n", resp_a, resp_b);
    
    if(valid == 1) {
        printf("Result: %u\n", result);
    } else {
        printf("Result: Invalid (divide by zero)\n");
    }
    
    close(sockfd);
    return 0;
}