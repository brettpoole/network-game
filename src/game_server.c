#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define SERVER_PORT 5000
#define MULTICAST_IP "239.0.0.1"
#define MULTICAST_PORT 6000
#define MAX_CLIENTS 32
#define GAME_TICK_RATE 1
#define BUFFER_SIZE 512

typedef struct {
    struct sockaddr_in addr;
    int health;
    float x, y;
} Player;

Player players[MAX_CLIENTS];
int player_count = 0;

int multicast_sock;
struct sockaddr_in multicast_addr;

// Initialize players
void init_players() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        players[i].health = 100;
        players[i].x = 0.0f;
        players[i].y = 0.0f;
    }
}

// Find or register player
int find_or_add_player(struct sockaddr_in *client_addr) {
    for (int i = 0; i < player_count; i++) {
        if (memcmp(&players[i].addr, client_addr, sizeof(struct sockaddr_in)) == 0) {
            return i;
        }
    }

    if (player_count < MAX_CLIENTS) {
        players[player_count].addr = *client_addr;
        return player_count++;
    }

    return -1; // Server full
}

// Process packets
uint8_t process_packet(int sockfd, struct sockaddr_in *client_addr, char *buffer) {
    int player_id = find_or_add_player(client_addr);
    if (player_id == -1) return 0;

    if (buffer[0] == 'M') {  // Move command
        sscanf(buffer + 1, "%f %f", &players[player_id].x, &players[player_id].y);
    } else if (buffer[0] == 'A') {  // Attack command
        int target_id;
        sscanf(buffer + 1, "%d", &target_id);
        if (target_id >= 0 && target_id < player_count) {
            players[target_id].health -= 10;
        }
    }
    return 1;
}

// Broadcast player states via multicast
void broadcast_updates() {
    char update_buffer[BUFFER_SIZE];
    int offset = 0;

    for (int i = 0; i < player_count; i++) {
        offset += snprintf(update_buffer + offset, BUFFER_SIZE - offset,
                           "P %d %.2f %.2f %d\n", i, players[i].x, players[i].y, players[i].health);
    }

    sendto(multicast_sock, update_buffer, strlen(update_buffer), 0,
           (struct sockaddr *)&multicast_addr, sizeof(multicast_addr));
}

// Game loop
void game_loop(int sockfd) {
    struct timeval last_tick, current_time;
    uint8_t packet_returned = 0;
    gettimeofday(&last_tick, NULL);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        char buffer[BUFFER_SIZE];

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        struct timeval timeout = {0, 1000}; 

        if (select(sockfd + 1, &read_fds, NULL, NULL, &timeout) > 0) {
            recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                     (struct sockaddr *)&client_addr, &addr_len);
            packet_returned = process_packet(sockfd, &client_addr, buffer);
        }

        gettimeofday(&current_time, NULL);
        double elapsed_ms = (current_time.tv_sec - last_tick.tv_sec) * 1000.0 +
                            (current_time.tv_usec - last_tick.tv_usec) / 1000.0;
        if (elapsed_ms >= (2000.0 / GAME_TICK_RATE) && packet_returned) {
            broadcast_updates();
            last_tick = current_time;
            packet_returned = 0;
        }
    }
}

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return perror("Socket"), EXIT_FAILURE;

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        return perror("Bind failed"), EXIT_FAILURE;

    // Setup multicast socket
    multicast_sock = socket(AF_INET, SOCK_DGRAM, 0);
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(MULTICAST_PORT);
    inet_pton(AF_INET, MULTICAST_IP, &multicast_addr.sin_addr);

    printf("Game server running on port %d\n", SERVER_PORT);
    init_players();
    game_loop(sockfd);

    close(sockfd);
    return 0;
}

