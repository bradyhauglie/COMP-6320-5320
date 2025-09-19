#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BACKLOG 10

int main() {
    int sockfd, new_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    char request[9];
    char response[14];
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    listen(sockfd, BACKLOG);
    printf("Server listening on port %d\n", PORT);
    
    while(1) {
        addr_len = sizeof(client_addr);
        new_fd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len);
        
        recv(new_fd, request, 9, 0);
        
        // Parse request
        char op = request[0];
        int a = ntohl(*(int*)(request + 1));
        int b = ntohl(*(int*)(request + 5));
        
        // Calculate result
        int result = 0;
        int valid = 1;
        
        switch(op) {
            case '+':
                result = a + b;
                break;
            case '-':
                result = a - b;
                break;
            case 'x':
                result = a * b;
                break;
            case '/':
                if(b == 0) {
                    result = 0;
                    valid = 2;
                } else {
                    result = a / b;
                }
                break;
        }
        
        response[0] = op;
        *(int*)(response + 1) = htonl(a);
        *(int*)(response + 5) = htonl(b);
        *(int*)(response + 9) = htonl(result);
        response[13] = valid;
        
        send(new_fd, response, 14, 0);
        close(new_fd);
    }
    
    close(sockfd);
    return 0;
}