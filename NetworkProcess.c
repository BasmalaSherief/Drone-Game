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


// Coordinate Inversion 
// Converts Top-Left (Ncurses) to Bottom-Left (Cartesian) and vice-versa
float invert_axis(float y) 
{ 
    return (float)(MAP_HEIGHT - 1) - y; 
}

// ROBUST BUFFERED RECEIVER
int recv_line(LinkContext *ctx, char *dest, int max_len) 
{
    int line_idx = 0;
    
    while (1) {
        // 1. Check if we have data in the internal buffer
        while (ctx->buf_start < ctx->buf_end) 
        {
            char c = ctx->net_buffer[ctx->buf_start++];
            
            if (c == '\n') 
            {
                dest[line_idx] = '\0'; // Null-terminate
                
                // Logging
                log_msg("NETWORK", "RX << '%s'", dest); 
                return line_idx; // Successfully got a full line
            }
            
            if (line_idx < max_len - 1) 
            {
                dest[line_idx++] = c;
            }
        }
        
        // 2. Buffer is empty or exhausted, read more from socket
        ctx->buf_start = 0;
        int n = read(ctx->conn_fd, ctx->net_buffer, BUFFER_CAP);
        
        if (n <= 0) 
        {
            log_msg("NETWORK", "Connection Lost or Socket Closed.");
            return -1; // Error or Disconnect
        }
        
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

    // Logging
    log_msg("NETWORK", "TX >> '%s'", payload); 

    // Append Newline
    strncat(payload, "\n", BUFFER_CAP - strlen(payload) - 1);
    return write(fd, payload, strlen(payload));
}

int establish_link(int role, const char *target_ip, int port) 
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        log_msg("NETWORK", "Error creating socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in s_addr = {0};
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);

    log_msg("NETWORK", "Connecting to %s:%d ...", target_ip, port);

    if (role == MODE_SERVER) 
    { 
        s_addr.sin_addr.s_addr = INADDR_ANY;
        int opt_val = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
        
        if (bind(sockfd, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) 
        {
            log_msg("NETWORK", "FATAL: Bind Failed (Port busy?)");
            return -1;
        }
        listen(sockfd, 1);
        
        log_msg("NETWORK", "Waiting for Client...");
        int client_fd = accept(sockfd, NULL, NULL);
        log_msg("NETWORK", "Client Accepted!");
        close(sockfd);
        return client_fd;
    } 
    else 
    { 
        struct hostent *host = gethostbyname(target_ip);
        if (!host) 
        {
            log_msg("NETWORK", "FATAL: Invalid Host/IP");
            return -1;
        }
        memcpy(&s_addr.sin_addr, host->h_addr_list[0], host->h_length);
        
        while (connect(sockfd, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) 
        {
            log_msg("NETWORK", "Retrying Connection...");
            sleep(RETRY_SEC);
        }
        log_msg("NETWORK", "Connected to Server!");
        return sockfd;
    }
}

void execute_handshake(LinkContext *ctx) 
{
    char rx_buf[BUFFER_CAP];
    log_msg("NETWORK", "Starting Handshake...");
    
    if (ctx->role == MODE_SERVER) 
    {
        // SERVER SIDE
        send_line(ctx->conn_fd, "ok");
        recv_line(ctx, rx_buf, BUFFER_CAP); // Expect "ook"
        
        // Send size with COMMA as per PDF
        send_line(ctx->conn_fd, "size %d, %d", MAP_WIDTH, MAP_HEIGHT);
        recv_line(ctx, rx_buf, BUFFER_CAP); // Expect "sok"
    } 
    else 
    {
        // CLIENT SIDE
        recv_line(ctx, rx_buf, BUFFER_CAP); // Wait for "ok"
        send_line(ctx->conn_fd, "ook");
        
        recv_line(ctx, rx_buf, BUFFER_CAP); // Wait for "size ..."
        
        // RESIZE LOGIC START
        // Parse dimensions to resize the client window
        int w = MAP_WIDTH; 
        int h = MAP_HEIGHT;
        if (sscanf(rx_buf, "size %d, %d", &w, &h) != 2) 
        {
             sscanf(rx_buf, "size %d %d", &w, &h); // Fallback for space
        }
        
        // Send "Magic Obstacle" to Blackboard to trigger resize
        Obstacle config_pkt[MAX_OBSTACLES];
        memset(config_pkt, 0, sizeof(config_pkt));
        config_pkt[0].x = w;
        config_pkt[0].y = h;
        config_pkt[0].active = 99; // 99 = Special Resize Flag
        write(ctx->pipe_out_fd, config_pkt, sizeof(config_pkt));
        log_msg("NETWORK", "Sent resize command (%dx%d) to Blackboard.", w, h);

        // Send Acknowledge with the received string
        char ack_msg[256];
        snprintf(ack_msg, sizeof(ack_msg), "sok %s", rx_buf);
        send_line(ctx->conn_fd, ack_msg);
    }
    log_msg("NETWORK", "Handshake Complete. Game ON.");
}

void process_traffic(LinkContext *ctx) 
{
    DroneState local_drone = {0};
    
    // We must write OBSTACLES to the blackboard, not DroneState 
    Obstacle remote_obs_pkt[MAX_OBSTACLES]; 
    
    char buffer[BUFFER_CAP];
    float x_in, y_in;

    // Set Pipe to Non-Blocking so game doesn't freeze network
    fcntl(ctx->pipe_in_fd, F_SETFL, O_NONBLOCK);

    while (1) 
    {
        // Drain local pipe to get latest local drone position
        while (read(ctx->pipe_in_fd, &local_drone, sizeof(DroneState)) > 0);

        if (ctx->role == MODE_SERVER) 
        {
            // SERVER LOOP
            // 1. Send Drone
            send_line(ctx->conn_fd, "drone");
            send_line(ctx->conn_fd, "%.2f %.2f", local_drone.x, invert_axis(local_drone.y));
            recv_line(ctx, buffer, BUFFER_CAP); // Ack "dok"

            // 2. Request Obstacle
            send_line(ctx->conn_fd, "obst");
            recv_line(ctx, buffer, BUFFER_CAP); // Obstacle Coords
            
            // 3. Process Received Data
            if(sscanf(buffer, "%f %f", &x_in, &y_in) == 2) 
            {
                // Prepare Blackboard Packet
                memset(remote_obs_pkt, 0, sizeof(remote_obs_pkt));
                remote_obs_pkt[0].x = (int)x_in;
                remote_obs_pkt[0].y = (int)invert_axis(y_in);
                remote_obs_pkt[0].active = 1;
                
                // Write to Blackboard
                write(ctx->pipe_out_fd, remote_obs_pkt, sizeof(remote_obs_pkt));
            }
            send_line(ctx->conn_fd, "pok");

        } else 
        {
            // CLIENT LOOP
            // 1. Receive Drone
            recv_line(ctx, buffer, BUFFER_CAP); // "drone" tag
            
            // Handle Quit Signal
            if (strncmp(buffer, "q", 1) == 0) 
            {
                 send_line(ctx->conn_fd, "qok");
                 log_msg("NETWORK", "Server requested quit.");
                 break;
            }

            recv_line(ctx, buffer, BUFFER_CAP); // Coords
            
            if(sscanf(buffer, "%f %f", &x_in, &y_in) == 2) 
            {
                memset(remote_obs_pkt, 0, sizeof(remote_obs_pkt));
                remote_obs_pkt[0].x = (int)x_in;
                remote_obs_pkt[0].y = (int)invert_axis(y_in);
                remote_obs_pkt[0].active = 1;
                write(ctx->pipe_out_fd, remote_obs_pkt, sizeof(remote_obs_pkt));
            }
            send_line(ctx->conn_fd, "dok");

            // 2. Send Obstacle (Local Drone)
            recv_line(ctx, buffer, BUFFER_CAP); // "obst" tag
            send_line(ctx->conn_fd, "%.2f %.2f", local_drone.x, invert_axis(local_drone.y));
            recv_line(ctx, buffer, BUFFER_CAP); // Ack "pok"
        }
        
        usleep(SYNC_RATE_US); 
    }
}

int main(int argc, char *argv[]) 
{
    // Arguments passed by BlackboardServer: <mode> <port> <ip>
    if (argc < 4) 
    {
        log_msg("NETWORK", "Error: Missing arguments.");
        return 1;
    }
    
    log_msg("NETWORK", "Process started");
    
    LinkContext ctx = {0};
    ctx.role = atoi(argv[1]);
    int port_num = atoi(argv[2]);
    char *ip_addr = argv[3];

    // SETUP NAMED PIPES
    // fifoBBObs -> Read Local Drone (Server sends it here)
    // fifoObsBB -> Write Remote Obstacle (Server reads it from here)
    
    ctx.pipe_in_fd = open("/tmp/fifoBBObs", O_RDONLY);
    ctx.pipe_out_fd = open("/tmp/fifoObsBB", O_WRONLY);

    if (ctx.pipe_in_fd < 0 || ctx.pipe_out_fd < 0) 
    {
        log_msg("NETWORK", "Error opening Named Pipes! Check Blackboard.");
        return 1;
    }

    // Connect
    ctx.conn_fd = establish_link(ctx.role, ip_addr, port_num);
    if (ctx.conn_fd < 0) return 1;

    // Run
    execute_handshake(&ctx);
    process_traffic(&ctx);
    
    close(ctx.conn_fd);
    close(ctx.pipe_in_fd);
    close(ctx.pipe_out_fd);
    return 0;
}