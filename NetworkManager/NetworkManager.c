#include <sys/socket.h>
#include <netinet/in.h>
#include "NetworkManager.h"

#define BUFFER_SIZE 256

// --- Coordinate Conversion ---
// Virtual system, origin at the bottom left
// Ncurses is Top-Left, We invert Y.
Point2D to_virtual(Point2D p, int map_height) 
{
    Point2D v;
    v.x = p.x;
    v.y = (float)map_height - p.y; 
    return v;
}

Point2D to_screen(Point2D v, int map_height) 
{
    Point2D p;
    p.x = v.x;
    p.y = (float)map_height - v.y;
    return p;
}

// --- INITIALIZATION ---
int network_init(NetworkContext *ctx, const char *ip, int port, int is_server) 
{
    ctx->is_server = is_server;
    ctx->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (is_server) 
    {
        // SERVER: Bind and Listen
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(ctx->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
        if (listen(ctx->socket_fd, 1) < 0) return -1;
        
        printf("Waiting for client connection on port %d...\n", port);
        int client_fd = accept(ctx->socket_fd, NULL, NULL);
        if (client_fd < 0) return -1;
        
        // Replace listener with client socket for communication
        close(ctx->socket_fd);
        ctx->socket_fd = client_fd;
    } 
    else 
    {
        // CLIENT: Connect
        if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) return -1;
        if (connect(ctx->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    }
    
    ctx->connected = 1;
    return 0;
}

void network_close(NetworkContext *ctx) 
{
    if (ctx->socket_fd >= 0) close(ctx->socket_fd);
    ctx->connected = 0;
}

// --- PROTOCOL: HANDSHAKE ---
// Server: snd "ok"; rcv "ook"
// Client: rcv "ok"; snd "ook"
int protocol_handshake(NetworkContext *ctx) 
{
    char buf[BUFFER_SIZE];
    
    if (ctx->is_server) 
    {
        // 1. Send "ok"
        sprintf(buf, "ok");
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);
        
        // 2. Receive "ook"
        recv(ctx->socket_fd, buf, BUFFER_SIZE, 0);
        if (strcmp(buf, "ook") != 0) return -1;
    } 
    else 
    {
        // 1. Receive "ok"
        recv(ctx->socket_fd, buf, BUFFER_SIZE, 0);
        if (strcmp(buf, "ok") != 0) return -1;
        
        // 2. Send "ook"
        sprintf(buf, "ook");
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);
    }
    return 0;
}

// --- PROTOCOL: WINDOW SIZE ---
// Server: snd "size w,h"; rcv "sok <size>"
// Client: rcv "size w,h"; snd "sok <size>"
int protocol_exchange_window_size(NetworkContext *ctx, int *width, int *height) 
{
    char buf[BUFFER_SIZE];
    
    if (ctx->is_server) 
    {
        // Send Dimensions
        sprintf(buf, "size %d,%d", *width, *height);
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);
        
        // Receive Ack
        recv(ctx->socket_fd, buf, BUFFER_SIZE, 0);
        // Requirement: "sok <size>"
        if (strncmp(buf, "sok", 3) != 0) return -1;
    } 
    else 
    {
        // Receive Dimensions
        recv(ctx->socket_fd, buf, BUFFER_SIZE, 0);
        sscanf(buf, "size %d,%d", width, height);
        
        // Send Ack
        sprintf(buf, "sok size"); 
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);
    }
    return 0;
}

// --- PROTOCOL: MAIN LOOP ---
int protocol_exchange_positions(NetworkContext *ctx, Point2D *local_drone, Point2D *remote_obstacle, int map_height) 
{
    char buf[BUFFER_SIZE];
    Point2D v_drone, v_obst;

    if (ctx->is_server) 
    {
        // --- SERVER SEQUENCE ---
        // 1. Send "drone" (header)
        sprintf(buf, "drone");
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);
        
        // 2. Send "x, y" (Virtual Coordinates)
        v_drone = to_virtual(*local_drone, map_height);
        sprintf(buf, "%.2f,%.2f", v_drone.x, v_drone.y);
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);
        
        // 3. Receive "dok <drone>" (Ack)
        recv(ctx->socket_fd, buf, BUFFER_SIZE, 0);
        if (strncmp(buf, "dok", 3) != 0) return -1;

        // 4. Send "obst" (Requesting obstacle)
        sprintf(buf, "obst");
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);
        
        // 5. Receive "x, y" (Virtual Coordinates of client drone)
        recv(ctx->socket_fd, buf, BUFFER_SIZE, 0);
        sscanf(buf, "%f,%f", &v_obst.x, &v_obst.y);
        *remote_obstacle = to_screen(v_obst, map_height); // Convert back for display
        
        // 6. Send "pok <obstacle>" (Ack)
        sprintf(buf, "pok obstacle");
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);

    } 
    else 
    {
        // --- CLIENT SEQUENCE ---      
        // 1. Receive Header (Expected "drone" or "q")
        recv(ctx->socket_fd, buf, BUFFER_SIZE, 0);
        
        if (strcmp(buf, "q") == 0) 
        {
             // Handle quit request
             sprintf(buf, "qok");
             send(ctx->socket_fd, buf, strlen(buf)+1, 0);
             return -2; // Signal to exit loop
        }
        
        if (strcmp(buf, "drone") == 0) 
        {
            // 2. Receive Server Drone Position
            recv(ctx->socket_fd, buf, BUFFER_SIZE, 0);
            sscanf(buf, "%f,%f", &v_obst.x, &v_obst.y);
            *remote_obstacle = to_screen(v_obst, map_height); // Server drone acts as obstacle for client

            // 3. Send "dok <drone>"
            sprintf(buf, "dok drone");
            send(ctx->socket_fd, buf, strlen(buf)+1, 0);
        }
        
        // 4. Receive "obst" request
        recv(ctx->socket_fd, buf, BUFFER_SIZE, 0); // "obst"
        
        // 5. Send Local Drone Position (as "x, y")
        v_drone = to_virtual(*local_drone, map_height);
        sprintf(buf, "%.2f,%.2f", v_drone.x, v_drone.y);
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);
        
        // 6. Receive "pok" Ack
        recv(ctx->socket_fd, buf, BUFFER_SIZE, 0);
    }
    
    return 0;
}

int protocol_send_quit(NetworkContext *ctx) 
{
    char buf[16];
    if (ctx->is_server) 
    {
        sprintf(buf, "q");
        send(ctx->socket_fd, buf, strlen(buf)+1, 0);
        recv(ctx->socket_fd, buf, sizeof(buf), 0); // Expect "qok"
    }
    return 0;
}