/*
 * client11c.c - Multi-process UDP Client for Lab 1.1
 * One process sends 1-10000, another receives and tracks statistics
 * Follows the Lab11-RFC protocol specification
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>

#define SERVERPORT "10010"
#define MAXBUFLEN 1038
#define MAX_NUMBERS 10000

// Protocol message structure
#pragma pack(1)
typedef struct {
    uint16_t length;      // Total message length in network byte order
    uint32_t seq_num;     // Sequence number in network byte order
    uint64_t timestamp;   // Timestamp in network byte order
    char message[1024];   // Variable length string
} protocol_msg_t;
#pragma pack()

// Shared data structure for statistics
typedef struct {
    int received[MAX_NUMBERS + 1];  // Track which numbers were received
    uint64_t send_times[MAX_NUMBERS + 1];
    uint64_t rtt_times[MAX_NUMBERS + 1];
    int total_received;
    uint64_t min_rtt;
    uint64_t max_rtt;
    uint64_t total_rtt;
} stats_t;

stats_t stats;

// Function to get current time in milliseconds since epoch
uint64_t get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void sender_process(int sockfd, struct addrinfo *servinfo) {
    protocol_msg_t msg;
    char num_str[20];
    int i;
    uint64_t send_time;
    
    printf("Sender: Starting to send numbers 1 to %d...\n", MAX_NUMBERS);
    
    for (i = 1; i <= MAX_NUMBERS; i++) {
        // Prepare message
        memset(&msg, 0, sizeof(msg));
        snprintf(num_str, sizeof(num_str), "%d", i);
        strcpy(msg.message, num_str);
        
        uint16_t msg_len = strlen(msg.message);
        uint16_t total_len = 2 + 4 + 8 + msg_len;
        
        msg.length = htons(total_len);
        msg.seq_num = htonl(i);
        send_time = get_timestamp_ms();
        stats.send_times[i] = send_time;
        msg.timestamp = htobe64(send_time);
        
        // Send the message
        if (sendto(sockfd, &msg, total_len, 0,
                servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
            perror("sender: sendto");
        }
        
        // Small delay to avoid overwhelming the network
        usleep(1000); // 1ms delay
    }
    
    printf("Sender: Finished sending all %d numbers\n", MAX_NUMBERS);
}

void receiver_process(int sockfd) {
    protocol_msg_t recv_msg;
    int numbytes;
    uint64_t recv_time;
    int num;
    int timeout_count = 0;
    struct timeval tv;
    
    // Set socket timeout
    tv.tv_sec = 5;  // 5 second timeout
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    // Initialize statistics
    memset(&stats, 0, sizeof(stats));
    stats.min_rtt = UINT64_MAX;
    
    printf("Receiver: Waiting for echoes...\n");
    
    while (stats.total_received < MAX_NUMBERS && timeout_count < 10) {
        numbytes = recvfrom(sockfd, &recv_msg, MAXBUFLEN, 0, NULL, NULL);
        
        if (numbytes == -1) {
            timeout_count++;
            printf("Receiver: Timeout #%d, continuing to wait...\n", timeout_count);
            continue;
        }
        
        timeout_count = 0; // Reset timeout counter
        recv_time = get_timestamp_ms();
        
        // Parse the received number
        num = atoi(recv_msg.message);
        
        if (num >= 1 && num <= MAX_NUMBERS) {
            if (!stats.received[num]) {
                stats.received[num] = 1;
                stats.total_received++;
                
                // Calculate round trip time
                if (stats.send_times[num] > 0) {
                    uint64_t rtt = recv_time - stats.send_times[num];
                    stats.rtt_times[num] = rtt;
                    
                    if (rtt < stats.min_rtt) stats.min_rtt = rtt;
                    if (rtt > stats.max_rtt) stats.max_rtt = rtt;
                    stats.total_rtt += rtt;
                }
            }
        }
        
        if (stats.total_received % 1000 == 0) {
            printf("Receiver: Received %d echoes so far...\n", stats.total_received);
        }
    }
    
    printf("Receiver: Finished receiving echoes\n");
}

void print_statistics() {
    int i;
    int missing_count = 0;
    double avg_rtt;
    
    printf("\n=== STATISTICS SUMMARY ===\n");
    
    // Find missing numbers
    printf("Missing echoes: ");
    for (i = 1; i <= MAX_NUMBERS; i++) {
        if (!stats.received[i]) {
            if (missing_count == 0) printf("%d", i);
            else printf(", %d", i);
            missing_count++;
            if (missing_count >= 10) {
                printf("... (showing first 10)");
                break;
            }
        }
    }
    
    if (missing_count == 0) {
        printf("None");
    }
    printf("\n");
    
    printf("Total sent: %d\n", MAX_NUMBERS);
    printf("Total received: %d\n", stats.total_received);
    printf("Missing: %d\n", missing_count);
    printf("Success rate: %.2f%%\n", 
           (double)stats.total_received / MAX_NUMBERS * 100);
    
    if (stats.total_received > 0) {
        avg_rtt = (double)stats.total_rtt / stats.total_received;
        printf("Smallest RTT: %lu ms\n", stats.min_rtt);
        printf("Largest RTT: %lu ms\n", stats.max_rtt);
        printf("Average RTT: %.2f ms\n", avg_rtt);
    }
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    pid_t child_pid;
    int status;

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

    // Fork to create sender and receiver processes
    child_pid = fork();
    
    if (child_pid == -1) {
        perror("fork");
        exit(1);
    } else if (child_pid == 0) {
        // Child process: sender
        sender_process(sockfd, p);
        exit(0);
    } else {
        // Parent process: receiver
        receiver_process(sockfd);
        
        // Wait for sender to finish
        wait(&status);
        
        // Print statistics
        print_statistics();
    }

    freeaddrinfo(servinfo);
    close(sockfd);
    return 0;
}