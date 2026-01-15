#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "common.h" 

/* NetworkProcess.c - Handlers Network Communication
   - Replaces printf with log_msg to avoid breaking Ncurses.
   - Implements strict handshake protocol (size w,h -> sok size w,h).
*/

// --- Coordinate Conversion ---
float to_virtual_y(float y) { return (float)(MAP_HEIGHT - 1) - y; }
float to_local_y(float y) { return (float)(MAP_HEIGHT - 1) - y; }

// --- Helper Functions ---
int send_msg(int sock, const char *fmt, ...) 
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    // Ensure newline for string-based protocol
    if (buf[strlen(buf)-1] != '\n') strcat(buf, "\n");
    return write(sock, buf, strlen(buf));
}

int recv_line(int sock, char *buf, int size) 
{
    int i = 0; char c;
    while (i < size - 1) 
    {
        if (read(sock, &c, 1) <= 0) return -1;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

int main(int argc, char *argv[]) 
{
    // Log startup
    log_msg("NETWORK", "Process started.");

    if (argc < 4) 
    {
        log_msg("NETWORK", "Error: Missing arguments.");
        return 1;
    }

    int mode = atoi(argv[1]); // 1=Server, 2=Client
    int port = atoi(argv[2]);
    char *ip = argv[3];

    // --- 1. SETUP NAMED PIPES ---
    const char *fifoBBObs = "/tmp/fifoBBObs"; // Read local drone
    const char *fifoObsBB = "/tmp/fifoObsBB"; // Write remote obstacle

    int pipe_rx = open(fifoBBObs, O_RDONLY | O_NONBLOCK);
    int pipe_tx = open(fifoObsBB, O_WRONLY | O_NONBLOCK);

    if (pipe_rx < 0 || pipe_tx < 0) 
    {
        log_msg("NETWORK", "Error: Failed to open pipes.");
        return 1;
    }

    // --- 2. SETUP SOCKET ---
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    log_msg("NETWORK", "Initializing as %s on port %d...", (mode == 1) ? "SERVER" : "CLIENT", port);

    if (mode == 1) 
    { // SERVER MODE
        addr.sin_addr.s_addr = INADDR_ANY;
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
        {
            log_msg("NETWORK", "Error: Bind failed.");
            return 1;
        }
        listen(sock, 1);
        
        log_msg("NETWORK", "Waiting for client connection...");
        int client_sock = accept(sock, NULL, NULL);
        if (client_sock < 0) {
            log_msg("NETWORK", "Error: Accept failed.");
            return 1;
        }
        
        close(sock); // Close listener
        sock = client_sock; // Use client connection
        log_msg("NETWORK", "Client connected.");

        // --- HANDSHAKE (Server side) ---
        // 1. Send "ok"
        send_msg(sock, "ok");
        
        // 2. Receive "ook"
        char buf[256]; 
        recv_line(sock, buf, 256); 
        if (strcmp(buf, "ook") != 0) log_msg("NETWORK", "Warning: Handshake 'ook' mismatch. Got: %s", buf);

        // 3. Send "size w h"
        // PDF Spec requires sending dimensions. Friend's code used space separator.
        send_msg(sock, "size %d %d", MAP_WIDTH, MAP_HEIGHT);
        
        // 4. Receive "sok size ..."
        recv_line(sock, buf, 256); 
        log_msg("NETWORK", "Handshake complete. Received ack: %s", buf);

    } 
    else 
    { // CLIENT MODE
        struct hostent *he = gethostbyname(ip);
        if (!he) 
        { 
            log_msg("NETWORK", "Error: Invalid IP address."); 
            return 1; 
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
        
        log_msg("NETWORK", "Connecting to %s...", ip);
        while(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
        {
            sleep(1); // Retry
        }
        log_msg("NETWORK", "Connected to Server.");

        // --- HANDSHAKE (Client side) ---
        char buf[256]; 
        
        // 1. Receive "ok"
        recv_line(sock, buf, 256); 
        
        // 2. Send "ook"
        send_msg(sock, "ook");
        
        // 3. Receive "size w h"
        recv_line(sock, buf, 256); // buf contains "size 80 24"
        log_msg("NETWORK", "Server Window: %s", buf);
        
        // 4. Send "sok size ..."
        char ack_msg[512];
        snprintf(ack_msg, sizeof(ack_msg), "sok %s", buf); 
        send_msg(sock, ack_msg);
    }

    log_msg("NETWORK", "Starting Game Loop.");

    // --- 3. GAME LOOP ---
    DroneState local_drone = {0};
    Obstacle obstacles[MAX_OBSTACLES]; 
    char buf[256];

    while(1) 
    {
        // 1. Read Local Drone Position (Drain Pipe)
        ssize_t r;
        while ((r = read(pipe_rx, &local_drone, sizeof(DroneState))) > 0) {
            // keep last valid drone state
        }
        

        if (mode == 1) 
        { // SERVER PROTOCOL LOOP
            // Send Drone
            send_msg(sock, "drone");
            send_msg(sock, "%.2f %.2f", local_drone.x, to_virtual_y(local_drone.y));
            recv_line(sock, buf, 256); // Receive "dok"

            // Request Obstacle
            send_msg(sock, "obst");
            recv_line(sock, buf, 256); // Receive "x y"
            
            float rx, ry; 
            sscanf(buf, "%f %f", &rx, &ry);
            
            send_msg(sock, "pok"); // Acknowledge

            // Update Blackboard
            for(int i=0; i<MAX_OBSTACLES; i++) obstacles[i].active = 0;
            obstacles[0].x = (int)rx;
            obstacles[0].y = (int)to_local_y(ry);
            obstacles[0].active = 1; 
            
            write(pipe_tx, obstacles, sizeof(obstacles));
        } 
        else 
        { // CLIENT PROTOCOL LOOP
            recv_line(sock, buf, 256); 
            
            if (strcmp(buf, "q") == 0) 
            {
                send_msg(sock, "qok");
                log_msg("NETWORK", "Server requested quit.");
                break;
            }
            
            if (strcmp(buf, "drone") == 0) 
            {
                recv_line(sock, buf, 256); 
                float rx, ry; 
                sscanf(buf, "%f %f", &rx, &ry);
                
                send_msg(sock, "dok");

                for(int i=0; i<MAX_OBSTACLES; i++) obstacles[i].active = 0;
                obstacles[0].x = (int)rx;
                obstacles[0].y = (int)to_local_y(ry);
                obstacles[0].active = 1;
                write(pipe_tx, obstacles, sizeof(obstacles));
            }

            recv_line(sock, buf, 256); // "obst"
            send_msg(sock, "%.2f %.2f", local_drone.x, to_virtual_y(local_drone.y));
            recv_line(sock, buf, 256); // "pok"
        }

        usleep(30000); // 30ms Tick
    }

    close(sock);
    close(pipe_rx);
    close(pipe_tx);
    log_msg("NETWORK", "Exiting.");
    return 0;
}