/*
** client11c.c -- multi-process UDP client for Lab 1.1
** Based on talker.c from Beej's guide
*/

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
#include <sys/wait.h>

#define SERVERPORT "10010"
#define MAXBUFLEN 1100
#define MAX_NUMS 10000

// simple function to get current time in milliseconds
long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec * 1000000 + tv.tv_usec / 1000);
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    char send_buf[MAXBUFLEN];
    char recv_buf[MAXBUFLEN];
    char num_str[20];
    pid_t child_pid;
    
    // for statistics
    int received[MAX_NUMS + 1];
    long long send_times[MAX_NUMS + 1];
    int total_received = 0;
    long long min_rtt = 999999;
    long long max_rtt = 0;
    long long total_rtt = 0;

    if (argc != 2) {
        fprintf(stderr,"usage: client11c hostname\n");
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

    // initialize arrays
    memset(received, 0, sizeof(received));
    memset(send_times, 0, sizeof(send_times));

    // fork to create two processes
    child_pid = fork();
    
    if (child_pid == 0) {
        // child process - sender
        printf("Sender: starting to send numbers 1 to %d\n", MAX_NUMS);
        
        for (int i = 1; i <= MAX_NUMS; i++) {
            // build message according to protocol
            sprintf(num_str, "%d", i);
            int string_len = strlen(num_str);
            int total_len = 2 + 4 + 8 + string_len;
            
            // convert to network order for packing
            unsigned short net_msg_len = htons(total_len);
            unsigned int net_seq_num = htonl(i);
            long long net_timestamp = get_time_ms();
            
            // pack the message
            memcpy(send_buf, &net_msg_len, 2);
            memcpy(send_buf + 2, &net_seq_num, 4);
            memcpy(send_buf + 6, &net_timestamp, 8);
            memcpy(send_buf + 14, num_str, string_len);

            send_times[i] = net_timestamp;

            // send it
            if (sendto(sockfd, send_buf, total_len, 0,
                     p->ai_addr, p->ai_addrlen) == -1) {
                perror("sendto");
            }
            
            usleep(1000);  // small delay
        }
        
        printf("Sender: finished sending all numbers\n");
        exit(0);
        
    } else {
        // parent process - receiver
        printf("Receiver: waiting for echoes\n");
        
        struct timeval tv;
        tv.tv_sec = 3;  // 3 second timeout
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        int timeouts = 0;
        int valid_rtts = 0;  // counter for valid RTT measurements
        
        while (total_received < MAX_NUMS && timeouts < 5) {
            numbytes = recvfrom(sockfd, recv_buf, MAXBUFLEN, 0, NULL, NULL);
            
            if (numbytes == -1) {
                timeouts++;
                printf("timeout #%d\n", timeouts);
                continue;
            }
            
            timeouts = 0;  // reset timeout counter
            long long recv_time = get_time_ms();
            
            // extract number and original timestamp from the message
            int num = atoi(recv_buf + 14);
            long long orig_timestamp;
            memcpy(&orig_timestamp, recv_buf + 6, 8);  // get timestamp from message
            
            // debug output for first few packets
            if (total_received < 5) {
                printf("DEBUG: packet %d, recv_time=%lld, orig_timestamp=%lld\n", 
                       num, recv_time, orig_timestamp);
            }
            
            if (num >= 1 && num <= MAX_NUMS && received[num] == 0) {
                received[num] = 1;
                total_received++;
                
                // calculate round trip time using timestamp from echoed message
                long long rtt = recv_time - orig_timestamp;
                
                // check if RTT makes sense (should be positive and reasonable)
                if (rtt > 0 && rtt < 10000) {  // less than 10 seconds seems reasonable
                    valid_rtts++;
                    if (rtt < min_rtt) min_rtt = rtt;
                    if (rtt > max_rtt) max_rtt = rtt;
                    total_rtt += rtt;
                } else {
                    if (total_received < 5) {
                        printf("DEBUG: invalid RTT %lld for packet %d\n", rtt, num);
                    }
                }
            }
            
            if (total_received % 1000 == 0) {
                printf("received %d echoes so far\n", total_received);
            }
        }
        
        wait(NULL);  // wait for child to finish
        
        // print statistics
        printf("\n--- STATISTICS ---\n");
        printf("Total sent: %d\n", MAX_NUMS);
        printf("Total received: %d\n", total_received);
        printf("Missing: %d\n", MAX_NUMS - total_received);
        printf("Valid RTT measurements: %d\n", valid_rtts);
        
        if (valid_rtts > 0) {
            printf("Smallest RTT: %lld ms\n", min_rtt);
            printf("Largest RTT: %lld ms\n", max_rtt);
            printf("Average RTT: %.2f ms\n", (double)total_rtt / valid_rtts / 1000.0);
        }
        
        // show some missing numbers
        printf("Some missing echoes: ");
        int shown = 0;
        for (int i = 1; i <= MAX_NUMS && shown < 10; i++) {
            if (received[i] == 0) {
                printf("%d ", i);
                shown++;
            }
        }
        printf("\n");
    }

    freeaddrinfo(servinfo);
    close(sockfd);
    return 0;
}