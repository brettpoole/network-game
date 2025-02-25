// File: game_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>

#define SERVER_PORT 5000
#define MAX_CLIENTS 32
#define GAME_TICK_RATE 60  // 60 updates per second
#define MAX_BUFFER_SIZE 512

typedef struct {
    struct sockaddr_in addr;
    int health;
    float x, y;
} Player;

Player players[MAX_CLIENTS];
int player_count = 0;

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
            return i; // Existing player
        }
    }

    if (player_count < MAX_CLIENTS) {
        players[player_count].addr = *client_addr;
        return player_count++;
    }
    
    return -1; // Server full
}

// Process incoming packets
void process_packet(int sockfd, struct sockaddr_in *client_addr, char *buffer) {
    int player_id = find_or_add_player(client_addr);
    if (player_id == -1) return; // Ignore if server full

    // Example: Simple protocol (M for move, A for attack)
    if (buffer[0] == 'M') {  // Move command
        sscanf(buffer + 1, "%f %f", &players[player_id].x, &players[player_id].y);
    } else if (buffer[0] == 'A') {  // Attack command
        int target_id;
        sscanf(buffer + 1, "%d", &target_id);
        if (target_id >= 0 && target_id < player_count) {
            players[target_id].health -= 10; // Apply damage
        }
    }
}

// Broadcast player states
void broadcast_updates(int sockfd) {
    char update_buffer[MAX_BUFFER_SIZE];
    int offset = 0;

    for (int i = 0; i < player_count; i++) {
        offset += snprintf(update_buffer + offset, MAX_BUFFER_SIZE - offset,
                           "P %d %.2f %.2f %d\n", i, players[i].x, players[i].y, players[i].health);
    }

    for (int i = 0; i < player_count; i++) {
        sendto(sockfd, update_buffer, strlen(update_buffer), 0,
               (struct sockaddr *)&players[i].addr, sizeof(players[i].addr));
    }
}

// Game loop with fixed tick rate
void game_loop(int sockfd) {
    struct timeval last_tick, current_time;
    gettimeofday(&last_tick, NULL);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        char buffer[MAX_BUFFER_SIZE];

        // Check for incoming packets (non-blocking)
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        struct timeval timeout = {0, 1000};  // 1ms timeout

        if (select(sockfd + 1, &read_fds, NULL, NULL, &timeout) > 0) {
            recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0,
                     (struct sockaddr *)&client_addr, &addr_len);
            process_packet(sockfd, &client_addr, buffer);
        }

        // Tick management
        gettimeofday(&current_time, NULL);
        double elapsed_ms = (current_time.tv_sec - last_tick.tv_sec) * 1000.0 +
                            (current_time.tv_usec - last_tick.tv_usec) / 1000.0;
        if (elapsed_ms >= (1000.0 / GAME_TICK_RATE)) {
            broadcast_updates(sockfd);
            last_tick = current_time;
        }
    }
}

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return EXIT_FAILURE;
    }

    printf("Game server running on port %d\n", SERVER_PORT);
    init_players();
    game_loop(sockfd);

    close(sockfd);
    return 0;
}

