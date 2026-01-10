#include "NetworkManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// Read configuration file
NetworkConfig* init_network_config(const char *config_file) 
{
    NetworkConfig *config = malloc(sizeof(NetworkConfig));
    config->mode = MODE_STANDALONE;
    strcpy(config->server_ip, "127.0.0.1");
    config->port = 5555;
    config->socket_fd = -1;
    config->connected = 0;
    
    FILE *f = fopen(config_file, "r");
    if (!f) {
        log_msg("NETWORK", "Config file not found, using defaults");
        return config;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f)) 
    {
        if (strncmp(line, "MODE=", 5) == 0) 
        {
            char mode_str[32];
            sscanf(line + 5, "%s", mode_str);
            if (strcmp(mode_str, "server") == 0) config->mode = MODE_SERVER;
            else if (strcmp(mode_str, "client") == 0) config->mode = MODE_CLIENT;
        }
        else if (strncmp(line, "SERVER_IP=", 10) == 0) 
        {
            sscanf(line + 10, "%s", config->server_ip);
        }
        else if (strncmp(line, "PORT=", 5) == 0) 
        {
            sscanf(line + 5, "%d", &config->port);
        }
    }
    
    fclose(f);
    log_msg("NETWORK", "Mode: %d, IP: %s, Port: %d", 
            config->mode, config->server_ip, config->port);
    return config;
}

// Server initialization
int network_server_init(NetworkConfig *config) 
{
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) 
    {
        perror("Socket creation failed");
        return -1;
    }
    
    // Allow port reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config->port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) 
    {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }
    
    // Listen
    if (listen(server_fd, 1) < 0) 
    {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }
    
    log_msg("NETWORK", "Server listening on port %d...", config->port);
    
    // Accept connection
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) 
    {
        perror("Accept failed");
        close(server_fd);
        return -1;
    }
    
    close(server_fd);
    config->socket_fd = client_fd;
    config->connected = 1;
    log_msg("NETWORK", "Client connected!");
    
    return client_fd;
}

// Client initialization
int network_client_init(NetworkConfig *config) 
{
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) 
    {
        perror("Socket creation failed");
        return -1;
    }
    
    // Connect to server
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->port);
    
    if (inet_pton(AF_INET, config->server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return -1;
    }
    
    log_msg("NETWORK", "Connecting to %s:%d...", config->server_ip, config->port);
    
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return -1;
    }
    
    config->socket_fd = sock;
    config->connected = 1;
    log_msg("NETWORK", "Connected to server!");
    
    return sock;
}

// Protocol implementations 
int server_handshake(int socket_fd) 
{
    // Send "ok"
    char msg[16] = "ok";
    if (send(socket_fd, msg, strlen(msg) + 1, 0) < 0) return -1;
    
    // Receive "ook"
    char response[16];
    if (recv(socket_fd, response, sizeof(response), 0) < 0) return -1;
    
    if (strcmp(response, "ook") != 0) 
    {
        log_msg("NETWORK", "Handshake failed: expected 'ook', got '%s'", response);
        return -1;
    }
    
    log_msg("NETWORK", "Handshake complete");
    return 0;
}

int client_handshake(int socket_fd) {
    // Receive "ok"
    char msg[16];
    if (recv(socket_fd, msg, sizeof(msg), 0) < 0) return -1;
    
    if (strcmp(msg, "ok") != 0) 
    {
        log_msg("NETWORK", "Handshake failed: expected 'ok', got '%s'", msg);
        return -1;
    }
    
    // Send "ook"
    char response[16] = "ook";
    if (send(socket_fd, response, strlen(response) + 1, 0) < 0) return -1;
    
    log_msg("NETWORK", "Handshake complete");
    return 0;
}

int server_send_size(int socket_fd, int width, int height) 
{
    char msg[64];
    snprintf(msg, sizeof(msg), "size %d,%d", width, height);
    if (send(socket_fd, msg, strlen(msg) + 1, 0) < 0) return -1;
    
    char response[64];
    if (recv(socket_fd, response, sizeof(response), 0) < 0) return -1;
    
    return (strncmp(response, "sok", 3) == 0) ? 0 : -1;
}

int client_receive_size(int socket_fd, int *width, int *height) {
    char msg[64];
    if (recv(socket_fd, msg, sizeof(msg), 0) < 0) return -1;
    
    if (sscanf(msg, "size %d,%d", width, height) != 2) return -1;
    
    char response[16] = "sok size";
    send(socket_fd, response, strlen(response) + 1, 0);
    
    return 0;
}
