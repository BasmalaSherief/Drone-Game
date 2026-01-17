#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <ncurses.h> 
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "Blackboard.h"
#include "../common.h"

/*  ASSIGNMENT1 CORRECTION:
        - fixed the killing through the handle_signal and global flag for cleaning
        which is found in the drone and keyboard files too 
        - Added error checking when dealing (making, opening, reading, writing) with pipes
        and 
*/

// GLOBAL FLAG FOR CLEANUP
volatile sig_atomic_t keep_running = 1;

// Operation Mode
int operation_mode = 0; // 0=Standalone, 1=Server, 2=Client

// Global PIDs to track children
pid_t pid_drone = 0;
pid_t pid_keyboard = 0;
pid_t pid_obst = 0;
pid_t pid_targ = 0;
pid_t pid_wd = 0;

void handle_signal(int sig) 
{
    keep_running = 0;
}

int main() 
{
    //Next 3 lines are from Assignment1 fixes
    // REGISTER SIGNALS
    signal(SIGINT, handle_signal);  // Ctrl+C
    signal(SIGTERM, handle_signal); // Kill command

    // Prevent crash or broken pipes
    signal(SIGPIPE, SIG_IGN);

    // Logging start of the main process to the log file
    log_msg("MAIN", "Process started with PID %d", getpid());
    srand(time(NULL));

    // CONFIGURATION
    // Read 'param.conf' to set operation_mode, IP, and Port
    FILE *f = fopen("param.conf", "r");
    char server_ip[32] = "127.0.0.1";
    int port = 5555; 

    if (f) 
    {
        char line[256];
        while (fgets(line, sizeof(line), f)) 
        {
            if (strstr(line, "MODE=server")) operation_mode = 1;
            if (strstr(line, "MODE=client")) operation_mode = 2;
            if (strstr(line, "SERVER_IP=")) sscanf(line, "SERVER_IP=%s", server_ip);
            if (strstr(line, "PORT=")) sscanf(line, "PORT=%d", &port);
        }
        fclose(f);
    }

    // PIPES + check for their errors
    const char *fifoDBB = "/tmp/fifoDBB";   
    const char *fifoBBD = "/tmp/fifoBBD";   
    const char *fifoBBDIS = "/tmp/fifoBBDIS"; 
    const char *fifoBBTar = "/tmp/fifoBBTar";   // Server -> Tar
    const char *fifoTarBB = "/tmp/fifoTarBB"; // Tar -> Server

    if (mkfifo(fifoDBB, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoDBB"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoBBD, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBD"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoBBDIS, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBDIS"); exit(EXIT_FAILURE); }
    if (mkfifo(FIFO_NET_RX, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBObs"); exit(EXIT_FAILURE); }
    if (mkfifo(FIFO_NET_TX, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoObsBB"); exit(EXIT_FAILURE); }
    if(operation_mode == 0) // Only create target pipes in standalone mode
    {
        if (mkfifo(fifoBBTar, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBTar"); exit(EXIT_FAILURE); }
        if (mkfifo(fifoTarBB, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoTarBB"); exit(EXIT_FAILURE); }
    }
    
    // The next code block is from the assigment1 fixes
    // LAUNCH CHILDREN 
    // ALWAYS launch Drone and Keyboard
    // Launch Drone
    char *arg_list_drone[] = { "./drone", NULL };
    pid_drone = spawn_process("./drone", arg_list_drone);
    log_msg("MAIN", "Launched Drone with PID: %d", pid_drone);

    // Launch Keyboard
    char *arg_list_kb[] = { "konsole", "-e", "./keyboard", NULL };
    pid_keyboard = spawn_process("konsole", arg_list_kb);
    log_msg("MAIN", "Launched Keyboard Manager with PID: %d", pid_keyboard);

    // CONDITIONALLY launch Generators and Watchdog
    // Server and client turn off the obstacle and target generators and the watchdog
    if (operation_mode == 0) // STANDALONE ONLY
    {
        // Launch Obstacle Process 
        char *arg_list_obs[] = { "./obstacle_process", NULL };
        pid_obst = spawn_process("./obstacle_process", arg_list_obs);
        log_msg("MAIN", "Launched Obstacle Process with PID: %d", pid_obst);

        // Launch Target Process 
        char *arg_list_tar[] = { "./target_process", NULL };
        pid_targ = spawn_process("./target_process", arg_list_tar);
        log_msg("MAIN", "Launched Target Process with PID: %d", pid_targ);

        char *arg_list_wd[] = { "./watchdog", NULL };
        pid_wd = spawn_process("./watchdog", arg_list_wd);
        log_msg("MAIN", "Launched Watchdog with PID: %d", pid_wd);
    }
    else // NETWORK MODE (SERVER OR CLIENT)
    {
        // Network Process
        char m[5], p[10];
        sprintf(m, "%d", operation_mode);
        sprintf(p, "%d", port);
        char *args_n[] = {"./network_process", m, p, server_ip, NULL};
        pid_obst = spawn_process("./network_process", args_n); // Reuse pid_obs
        log_msg("MAIN", "Launched Network Process with PID: %d (IP: %s)", pid_obst, server_ip);
    }

    // Non-blocking open for pipes 
    int fd_DBB = open(fifoDBB, O_RDONLY | O_NONBLOCK);
    if (fd_DBB == -1) { endwin(); perror("open read"); exit(1); }
  
    int fd_BBD = open(fifoBBD, O_WRONLY); 
    if (fd_BBD == -1) { endwin(); perror("open write BBDIS"); exit(1); }

    int fd_BBDIS = open(fifoBBDIS, O_WRONLY);
    if (fd_BBDIS == -1) { endwin(); perror("open write BBDIS"); exit(1); }

    // Network Shared Pipes
    int fd_NetTX = open(FIFO_NET_TX, O_WRONLY);
    if (fd_NetTX == -1) { endwin(); perror("open write NetTX"); exit(1); }
    int fd_NetRX = open(FIFO_NET_RX, O_RDONLY);
    if (fd_NetRX == -1) { endwin(); perror("open read NetRX"); exit(1); }

    int fd_BBTar = open(fifoBBTar, O_WRONLY );
    if (fd_BBTar == -1) { endwin(); perror("open write BBTar"); exit(1); }
  
    int fd_TarBB = open(fifoTarBB, O_RDONLY); 
    if (fd_TarBB == -1) { endwin(); perror("open read TarBB"); exit(1); }
    
    // NCURSES INIT
    init_console();

    // CLIENT MODE WINDOW RESIZING
    if (operation_mode == 2) 
    {
        log_msg("MAIN", "Waiting for Server Handshake...");
        Obstacle pkt[MAX_OBSTACLES];
        // Blocking read until network finishes handshake
        ssize_t r = read(fd_NetRX, pkt, sizeof(pkt));
        if(r > 0 && pkt[0].active == RESIZE_FLAG) 
        {
            resizeterm(pkt[0].y, pkt[0].x);
            wresize(stdscr, pkt[0].y, pkt[0].x);
            erase(); refresh();
            log_msg("MAIN", "Resized window to %dx%d", pkt[0].x, pkt[0].y);
        }
        else 
        {
            log_msg("MAIN", "Warning: Did not receive valid resize packet from NetworkProcess.");
        }
    }

    // DATA INIT 
    WorldState world = {0};
    world.drone.x = MAP_WIDTH / 2.0; 
    world.drone.y = MAP_HEIGHT / 2.0;
    world.drone.vx = 0; world.drone.vy = 0;
    world.score = 0;
    world.game_active = 0;
    TargetPacket tar_pkt;

    for(int i=0; i<MAX_OBSTACLES; i++) world.obstacles[i].active = 0;
    for(int i=0; i<MAX_TARGETS; i++) world.targets[i].active = 0;

    while(keep_running) 
    {
        // Send heartbeat to the watchdog
        if (pid_wd > 0) 
        {
            kill(pid_wd, SIGUSR1);
        }

        // READ INPUT (From Local Drone Controller)
        DroneState incoming_drone_state;
        ssize_t bytesRead = read(fd_DBB, &incoming_drone_state, sizeof(DroneState));

        if (bytesRead == -1) 
        {
            if (errno != EAGAIN) // no data right now
            { 
                perror("Server: Error reading from Drone Pipe (fifoDBB)");
            }
        } 
        else if (bytesRead == 0)  // Pipe is closed
        {
            // EOF handling: DO NOT EXIT IMMEDIATELY, let loop finish
            log_msg("SERVER", "Drone disconnected.");
            keep_running = 0;
        } 

        if (bytesRead > 0) 
        {
            if (incoming_drone_state.x == -1.0)
            {
                log_msg("SERVER", "Detected Quit Signal from Drone.");
                keep_running = 0; // Trigger cleanup
                continue;
            } 

            if (incoming_drone_state.x == -2.0) 
            {
                // Reset World State
                world.score = 0;
                world.game_active = 0;

                // Clear Arrays (Set active = 0)
                for(int i=0; i<MAX_OBSTACLES; i++) world.obstacles[i].active = 0;
                for(int i=0; i<MAX_TARGETS; i++) world.targets[i].active = 0;
                
                // Reset Drone Position visually
                world.drone.x = 10.0;
                world.drone.y = 10.0;

                continue; 
            }
            // Update Blackboard State
            world.drone = incoming_drone_state;
        }

        // CORE LOGIC
        write(fd_NetTX, &world.drone, sizeof(DroneState));
        read(fd_NetRX, world.obstacles, sizeof(world.obstacles));

        // TARGETS (Standalone Only)
        if (operation_mode == 0) 
        {
            write(fd_BBTar, &world.drone, sizeof(DroneState));
            read(fd_TarBB, &tar_pkt, sizeof(TargetPacket));
            memcpy(world.targets, tar_pkt.targets, sizeof(world.targets));
            world.score += tar_pkt.score_increment;
        }
        else 
        {
            // Network mode: No targets required by assignment spec
            // Ensure they remain inactive so they don't get drawn
            for(int i=0; i<MAX_TARGETS; i++) world.targets[i].active = 0;
        }

        // DISPLAY
        draw_map(&world);

        // BROADCAST 
        // WRITING OBSTACLES TO DRONE 
        ssize_t bytesWrittenBBD = write(fd_BBD, world.obstacles, sizeof(world.obstacles));
        if (bytesWrittenBBD == -1) 
        {
            if (errno == EPIPE) 
            {
                log_msg("SERVER", "Drone process disconnected (EPIPE). Stopping.");
                keep_running = 0;
            } 
            else 
            {
                log_msg("SERVER", "Error writing to Drone Pipe: %s", strerror(errno));
            }
        }

        // WRITING TO KEYBOARD DISPLAY
        ssize_t bytesWrittenBBDIS = write(fd_BBDIS, &world, sizeof(WorldState));
        if (bytesWrittenBBDIS == -1) 
        {
            if (errno == EPIPE) 
            {
                log_msg("SERVER", "Keyboard process disconnected (EPIPE). Stopping.");
                keep_running = 0;
            } 
            else if (errno != EAGAIN) 
            { 
                log_msg("SERVER", "Error writing to Display Pipe: %s", strerror(errno));
            }
        }

        usleep(30000); 
    }

    // CLEANUP
    log_msg("MAIN", "Stopping system...");

    // Close pipes
    close(fd_DBB);
    close(fd_BBD);
    close(fd_BBDIS);
    close(fd_NetTX);
    close(fd_NetRX);
    close(fd_BBTar);
    close(fd_TarBB);

    // Kill children using their PIDs
    if (pid_drone > 0) kill(pid_drone, SIGTERM);
    if (pid_keyboard > 0) kill(pid_keyboard, SIGTERM);
    if (pid_obst > 0) kill(pid_obst, SIGTERM);
    if (pid_targ > 0) kill(pid_targ, SIGTERM);
    
    // Wait for them to finish to avoid zombies
    waitpid(pid_drone, NULL, 0);
    waitpid(pid_keyboard, NULL, 0);
    waitpid(pid_obst, NULL, 0);
    waitpid(pid_targ, NULL, 0);

    // Destroy Ncurses window
    endwin();  

    // Unlink pipes so they don't persist
    unlink(fifoDBB);
    unlink(fifoBBD);
    unlink(fifoBBDIS);
    unlink(FIFO_NET_TX);
    unlink(FIFO_NET_RX);
    unlink(fifoBBTar);
    unlink(fifoTarBB);

    // Force kill group to ensure terminal windows close
    system("pkill -f drone");
    system("pkill -f keyboard");
    system("pkill -f obstacle_process");
    system("pkill -f target_process");
    system("pkill -f network_process");

    // Logging end of the main process to the log file
    log_msg("MAIN", "Clean exit. Bye!");

    return 0;
}