#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "../common.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


// NETWORK INITIALIZATION

// Initialize network configuration from params file
NetworkConfig* init_network_config(const char *config_file);

// Server: Create listening socket and wait for client
int network_server_init(NetworkConfig *config);

// Client: Connect to server
int network_client_init(NetworkConfig *config);


// PROTOCOL FUNCTIONS

// Server: Send initial handshake "ok" and wait for "ook"
int server_handshake(int socket_fd);

// Client: Receive "ok" and send "ook"
int client_handshake(int socket_fd);

// Server: Send window size "size w,h" and wait for "sok"
int server_send_size(int socket_fd, int width, int height);

// Client: Receive size and send "sok"
int client_receive_size(int socket_fd, int *width, int *height);

// Server: Send drone position
int server_send_drone(int socket_fd, float x, float y);

// Client: Receive drone position
int client_receive_drone(int socket_fd, float *x, float *y);

// Server: Receive obstacle position (client's drone)
int server_receive_obstacle(int socket_fd, float *x, float *y);

// Client: Send obstacle position (own drone)
int client_send_obstacle(int socket_fd, float x, float y);

// Send quit command
int send_quit(int socket_fd);

// UTILITY FUNCTIONS
// Close network connection gracefully
void network_cleanup(NetworkConfig *config);

#endif // NETWORKMANAGER_H