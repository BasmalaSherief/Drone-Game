#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Configuration for the network connection
typedef struct {
    int socket_fd;
    int is_server;
    int connected;
} NetworkContext;

// Coordinate point structure
typedef struct {
    float x;
    float y;
} Point2D;

// --- INITIALIZATION ---
// Initialize network (Server listens, Client connects)
int network_init(NetworkContext *ctx, const char *ip, int port, int is_server);
void network_close(NetworkContext *ctx);

// --- PROTOCOL HANDLERS ---
// Handshake: "ok" <-> "ook"
int protocol_handshake(NetworkContext *ctx);

// Window Size: "size w,h" <-> "sok size"
// Returns 0 on success, -1 on failure
int protocol_exchange_window_size(NetworkContext *ctx, int *width, int *height);

// Game Loop Exchange
// Server sends Drone, receives Obstacle. Client receives Drone, sends Obstacle.
// Performs coordinate conversion (Virtual <-> Screen) automatically.
int protocol_exchange_positions(NetworkContext *ctx, Point2D *local_drone, Point2D *remote_obstacle, int map_height);

// Termination: "q" <-> "qok"
int protocol_send_quit(NetworkContext *ctx);

#endif