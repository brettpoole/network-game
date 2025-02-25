#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5000
#define MULTICAST_IP "239.0.0.1"
#define MULTICAST_PORT 6000
#define BUFFER_SIZE 512

void *receive_updates(void *arg) {
    int sock = *(int *)arg;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    while (1) {
        int recv_len = recvfrom(sock, buffer, BUFFER_SIZE, 0, 
                                (struct sockaddr *)&sender_addr, &addr_len);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            printf("\n[UPDATE]: %s\n", buffer);
        }
    }
}

int main() {
    // Command socket for sending input
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return perror("Socket"), 1;

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Multicast socket for receiving updates
    int multicast_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (multicast_sock < 0) return perror("Multicast Socket"), 1;

    // Allow multiple clients to bind
    int reuse = 1;
    setsockopt(multicast_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

#ifdef SO_REUSEPORT
    setsockopt(multicast_sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

    struct sockaddr_in multicast_addr = {0};
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(MULTICAST_PORT);
    multicast_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(multicast_sock, (struct sockaddr *)&multicast_addr, sizeof(multicast_addr)) < 0) {
        return perror("Bind failed"), 1;
    }

    // Join multicast group
    struct ip_mreq mreq;
    inet_pton(AF_INET, MULTICAST_IP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(multicast_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        return perror("Multicast join failed"), 1;
    }

    // Create thread to listen for updates
    pthread_t thread;
    pthread_create(&thread, NULL, receive_updates, &multicast_sock);

    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(server_addr);

    while (1) {
        printf("Move (M x y) / Attack (A id): ");
        fgets(buffer, BUFFER_SIZE, stdin);
        sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&server_addr, addr_len);
    }

    close(sockfd);
    close(multicast_sock);
    return 0;
}

