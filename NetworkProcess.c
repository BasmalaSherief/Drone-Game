#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include "common.h" 

/* NetworkProcess.c - ROBUST VERSION
   - Uses Ring Buffer to handle TCP fragmentation
   - Handles Assignment 3 Handshake (size w, h)
   - Triggers Client Window Resize via Blackboard
*/


// Helpers 
float to_virtual_y(float y) { return (float)(MAP_HEIGHT - 1) - y; }
float to_local_y(float y) { return (float)(MAP_HEIGHT - 1) - y; }

// ROBUST RECEIVER (The Fix for Fragmentation)
int recv_line(LinkContext *ctx, char *dest, int max_len) 
{
    int line_idx = 0;
    while (1) {
        // 1. Drain internal buffer
        while (ctx->buf_start < ctx->buf_end) {
            char c = ctx->net_buffer[ctx->buf_start++];
            if (c == '\n') 
            {
                dest[line_idx] = '\0'; 
                return line_idx; 
            }
            if (line_idx < max_len - 1) dest[line_idx++] = c;
        }
        // 2. Refill buffer from socket
        ctx->buf_start = 0;
        int n = read(ctx->conn_fd, ctx->net_buffer, BUFFER_CAP);
        if (n <= 0) return -1; // Disconnect or Error
        ctx->buf_end = n;
    }
}

ssize_t send_line(int fd, const char *format, ...) 
{
    char payload[BUFFER_CAP];
    va_list args;
    va_start(args, format);
    vsnprintf(payload, sizeof(payload), format, args);
    va_end(args);
    strncat(payload, "\n", BUFFER_CAP - strlen(payload) - 1);
    return write(fd, payload, strlen(payload));
}

int establish_link(int role, const char *target_ip, int port) 
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in s_addr = {0};
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);

    if (role == 1) { // SERVER
        s_addr.sin_addr.s_addr = INADDR_ANY;
        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        bind(sockfd, (struct sockaddr *)&s_addr, sizeof(s_addr));
        listen(sockfd, 1);
        log_msg("NET", "Waiting for client on port %d...", port);
        int c = accept(sockfd, NULL, NULL);
        close(sockfd);
        return c;
    } else { // CLIENT
        struct hostent *h = gethostbyname(target_ip);
        if(!h) return -1;
        memcpy(&s_addr.sin_addr, h->h_addr_list[0], h->h_length);
        while (connect(sockfd, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) 
        {
            log_msg("NET", "Connecting to %s...", target_ip);
            sleep(RETRY_SEC);
        }
        return sockfd;
    }
}

int main(int argc, char *argv[]) 
{
    if (argc < 4) 
    {
        log_msg("NET", "Usage: ./network_process <mode> <port> <ip>");
        return 1;
    }

    LinkContext ctx = {0};
    ctx.role = atoi(argv[1]);
    int port = atoi(argv[2]);
    char *ip = argv[3];

    // Open Pipes defined in common.h
    ctx.pipe_in_fd = open(FIFO_NET_TX, O_RDONLY); // Read Local Drone
    ctx.pipe_out_fd = open(FIFO_NET_RX, O_WRONLY); // Write Remote Obstacle

    if (ctx.pipe_in_fd < 0 || ctx.pipe_out_fd < 0) 
    {
        log_msg("NET", "Error: Could not open named pipes. Is Server running?");
        return 1;
    }

    ctx.conn_fd = establish_link(ctx.role, ip, port);
    if(ctx.conn_fd < 0) return 1;

    // HANDSHAKE
    char buf[BUFFER_CAP];
    if (ctx.role == 1) 
    { // SERVER HANDSHAKE
        send_line(ctx.conn_fd, "ok");
        recv_line(&ctx, buf, 1024); // "ook"
        send_line(ctx.conn_fd, "size %d, %d", MAP_WIDTH, MAP_HEIGHT); // Note the comma
        recv_line(&ctx, buf, 1024); // "sok ..."
    } 
    else 
    { // CLIENT HANDSHAKE
        recv_line(&ctx, buf, 1024); // "ok"
        send_line(ctx.conn_fd, "ook");
        recv_line(&ctx, buf, 1024); // "size w, h"
        
        // PARSE SIZE & SEND RESIZE COMMAND TO BLACKBOARD
        int w=80, h=24;
        if(sscanf(buf, "size %d, %d", &w, &h) != 2) sscanf(buf, "size %d %d", &w, &h);
        
        Obstacle resize_pkt[MAX_OBSTACLES] = {0};
        resize_pkt[0].x = w; 
        resize_pkt[0].y = h; 
        resize_pkt[0].active = RESIZE_FLAG; // The Magic Flag
        write(ctx.pipe_out_fd, resize_pkt, sizeof(resize_pkt));
        
        send_line(ctx.conn_fd, "sok %d %d", w, h);
    }

    // MAIN LOOP
    DroneState local = {0};
    Obstacle remote[MAX_OBSTACLES];
    fcntl(ctx.pipe_in_fd, F_SETFL, O_NONBLOCK); // Non-blocking read from local game

    while(1) 
    {
        // Drain local pipe to get freshest drone position
        while(read(ctx.pipe_in_fd, &local, sizeof(DroneState)) > 0);

        if (ctx.role == 1) 
        { // SERVER BEHAVIOR
            send_line(ctx.conn_fd, "drone");
            send_line(ctx.conn_fd, "%.2f %.2f", local.x, to_virtual_y(local.y));
            recv_line(&ctx, buf, 1024); // "dok"

            send_line(ctx.conn_fd, "obst");
            recv_line(&ctx, buf, 1024); // "x y"
            
            float rx, ry; 
            sscanf(buf, "%f %f", &rx, &ry);
            
            // Send Remote Drone (as obstacle) to Blackboard
            memset(remote, 0, sizeof(remote));
            remote[0].x = (int)rx; 
            remote[0].y = (int)to_local_y(ry); 
            remote[0].active = 1;
            write(ctx.pipe_out_fd, remote, sizeof(remote));
            
            send_line(ctx.conn_fd, "pok");

        } 
        else 
        { // CLIENT BEHAVIOR
            recv_line(&ctx, buf, 1024); // "drone"
            if(buf[0] == 'q') { send_line(ctx.conn_fd, "qok"); break; }
            
            recv_line(&ctx, buf, 1024); // Coords
            float rx, ry; 
            sscanf(buf, "%f %f", &rx, &ry);
            
            // Send Remote Drone to Blackboard
            memset(remote, 0, sizeof(remote));
            remote[0].x = (int)rx; 
            remote[0].y = (int)to_local_y(ry); 
            remote[0].active = 1;
            write(ctx.pipe_out_fd, remote, sizeof(remote));
            
            send_line(ctx.conn_fd, "dok");

            recv_line(&ctx, buf, 1024); // "obst"
            send_line(ctx.conn_fd, "%.2f %.2f", local.x, to_virtual_y(local.y));
            recv_line(&ctx, buf, 1024); // "pok"
        }
        usleep(SYNC_RATE_US);
    }
    return 0;
}