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

/* We reuse the Named Pipes usually meant for the Obstacle Generator.
   - Reads: Local Drone Position (from fifoBBObs)
   - Writes: Remote Drone Position as an Obstacle (to fifoObsBB)
*/

// Coordinate Conversion 
// Virtual System: Origin Bottom-Left. Ncurses: Top-Left.
float to_virtual_y(float y) { return (float)(MAP_HEIGHT - 1) - y; }
float to_local_y(float y) { return (float)(MAP_HEIGHT - 1) - y; }

// Helper Functions 
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
    while (i < size - 1) {
        if (read(sock, &c, 1) <= 0) return -1;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

int main(int argc, char *argv[]) 
{
    if (argc < 4) 
    {
        fprintf(stderr, "Usage: %s <mode> <port> <ip>\n", argv[0]);
        return 1;
    }

    int mode = atoi(argv[1]); // 1=Server, 2=Client
    int port = atoi(argv[2]);
    char *ip = argv[3];

    // --- 1. SETUP NAMED PIPES ---
    // We use the existing pipes defined in your architecture
    const char *fifoBBObs = "/tmp/fifoBBObs"; // Read local drone
    const char *fifoObsBB = "/tmp/fifoObsBB"; // Write remote obstacle

    int pipe_rx = open(fifoBBObs, O_RDONLY | O_NONBLOCK);
    int pipe_tx = open(fifoObsBB, O_WRONLY);

    if (pipe_rx < 0 || pipe_tx < 0) 
    {
        perror("NetworkProcess: Failed to open pipes");
        return 1;
    }

    // --- 2. SETUP SOCKET ---
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    printf("[Network] Initializing as %s...\n", (mode == 1) ? "SERVER" : "CLIENT");

    if (mode == 1) 
    { // SERVER
        addr.sin_addr.s_addr = INADDR_ANY;
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
        {
            perror("Bind failed"); return 1;
        }
        listen(sock, 1);
        
        printf("[Network] Waiting for client...\n");
        int client_sock = accept(sock, NULL, NULL);
        close(sock); // Close listener
        sock = client_sock; // Use client connection

        // --- HANDSHAKE (Server side) ---
        send_msg(sock, "ok");
        char buf[256]; recv_line(sock, buf, 256); // Expect "ook"
        if (strcmp(buf, "ook") != 0) fprintf(stderr, "Handshake Error: Expected ook, got %s\n", buf);

        send_msg(sock, "size %d %d", MAP_WIDTH, MAP_HEIGHT);
        recv_line(sock, buf, 256); // Expect "sok <size>"

    } 
    else 
    { // CLIENT
        struct hostent *he = gethostbyname(ip);
        if (!he) { fprintf(stderr, "Invalid IP\n"); return 1; }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
        
        printf("[Network] Connecting to %s...\n", ip);
        while(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
        {
            sleep(1); // Retry
        }

        // --- HANDSHAKE (Client side) ---
        char buf[256]; recv_line(sock, buf, 256); // Expect "ok"
        send_msg(sock, "ook");
        
        recv_line(sock, buf, 256); // Expect "size w h"
        // (Optional: Parse size and resize window if needed)
        send_msg(sock, "sok size");
    }

    printf("[Network] Connection Established. Starting Game Loop.\n");

    // --- 3. GAME LOOP ---
    DroneState local_drone = {0};
    Obstacle obstacles[MAX_OBSTACLES]; // Your obstacle array format
    char buf[256];

    while(1) 
    {
        // 1. Read Local Drone Position from Pipe
        // We loop to drain the pipe and get the *latest* position (Friend's logic)
        int got_data = 0;
        while(read(pipe_rx, &local_drone, sizeof(DroneState)) > 0) 
        {
            got_data = 1;
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
            
            float rx, ry; sscanf(buf, "%f %f", &rx, &ry);
            
            send_msg(sock, "pok"); // Acknowledge

            // Write to Blackboard Pipe
            // We package the single remote drone as obstacles[0]
            for(int i=0; i<MAX_OBSTACLES; i++) obstacles[i].active = 0;
            
            obstacles[0].x = (int)rx;
            obstacles[0].y = (int)to_local_y(ry);
            obstacles[0].active = 1; // It's alive!
            
            write(pipe_tx, obstacles, sizeof(obstacles));

        } 
        else 
        { // CLIENT PROTOCOL LOOP
            recv_line(sock, buf, 256); 
            
            if (strcmp(buf, "q") == 0) 
            { // Quit Signal
                send_msg(sock, "qok");
                break;
            }
            
            if (strcmp(buf, "drone") == 0) 
            {
                // Receive Server Drone
                recv_line(sock, buf, 256); 
                float rx, ry; sscanf(buf, "%f %f", &rx, &ry);
                
                send_msg(sock, "dok");

                // Update local obstacle array with Server's position
                for(int i=0; i<MAX_OBSTACLES; i++) obstacles[i].active = 0;
                obstacles[0].x = (int)rx;
                obstacles[0].y = (int)to_local_y(ry);
                obstacles[0].active = 1;
                write(pipe_tx, obstacles, sizeof(obstacles));
            }

            // Handle Obstacle Request
            recv_line(sock, buf, 256); // "obst"
            send_msg(sock, "%.2f %.2f", local_drone.x, to_virtual_y(local_drone.y));
            recv_line(sock, buf, 256); // "pok"
        }

        usleep(30000); // 30ms Tick
    }

    close(sock);
    close(pipe_rx);
    close(pipe_tx);
    return 0;
}