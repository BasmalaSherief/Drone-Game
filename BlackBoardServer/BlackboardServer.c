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
#include "../NetworkManager/NetworkManager.h"
#include "../common.h"

/*  ASSIGNMENT1 CORRECTION:
        - fixed the killing through the handle_signal and global flag for cleaning
        which is found in the drone and keyboard files too 
        - Added error checking when dealing (making, opening, reading, writing) with pipes
        and 
*/

// GLOBAL FLAG FOR CLEANUP
volatile sig_atomic_t keep_running = 1;

// Global network context 
NetworkContext net_ctx;
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

    // NETWORK SETUP CONFIGURATION
    // Read 'param.conf' here to set operation_mode and IP
    FILE *f = fopen("param.conf", "r");
    char server_ip[32] = "127.0.0.1";
    int port = 5555; // Default

    if (f) 
    {
        char line[256];
        while (fgets(line, sizeof(line), f)) 
        {
            if (strstr(line, "MODE=server")) operation_mode = 1;
            if (strstr(line, "MODE=client")) operation_mode = 2;
            if (strstr(line, "SERVER_IP=")) sscanf(line, "SERVER_IP=%s", server_ip);
            
            // Read the port from the file
            if (strstr(line, "PORT=")) sscanf(line, "PORT=%d", &port);
        }
        fclose(f);
    }

    // DATA INIT 
    WorldState world;
    world.drone.x = MAP_WIDTH / 2.0;
    world.drone.y = MAP_HEIGHT / 2.0;
    world.drone.vx = 0; world.drone.vy = 0;
    world.score = 0;
    world.game_active = 0;

    for(int i=0; i<MAX_OBSTACLES; i++) world.obstacles[i].active = 0;
    for(int i=0; i<MAX_TARGETS; i++) world.targets[i].active = 0;

    // GAME COUNTERS
    int targets_spawned_total = 0;
    int targets_collected_total = 0;

    // PIPES + check for their errors
    const char *fifoDBB = "/tmp/fifoDBB";   
    const char *fifoBBD = "/tmp/fifoBBD";   
    const char *fifoBBDIS = "/tmp/fifoBBDIS"; 
    const char *fifoBBObs = "/tmp/fifoBBObs";   // Server -> Obs
    const char *fifoObsBB = "/tmp/fifoObsBB"; // Obs -> Server
    const char *fifoBBTar = "/tmp/fifoBBTar";   // Server -> Tar
    const char *fifoTarBB = "/tmp/fifoTarBB"; // Tar -> Server

    if (mkfifo(fifoDBB, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoDBB"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoBBD, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBD"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoBBDIS, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBDIS"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoBBObs, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBObs"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoObsBB, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoObsBB"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoBBTar, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBTar"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoTarBB, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoTarBB"); exit(EXIT_FAILURE); }
    
    // The next code block is from the assigment1 fixes
    // LAUNCH CHILDREN 
    // ALWAYS launch Drone and Keyboard
    // Launch Drone
    char *arg_list_drone[] = { "./drone", NULL };
    pid_drone = spawn_process("./drone", arg_list_drone);
    log_msg("MAIN", "Launched Drone with PID: %d", pid_drone);

    // Launch Keyboard
    // pass the keyboard executable as an argument (konsole -e ./keyboard)
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
    else
    {
        log_msg("MAIN", "Network Mode: Generators and Watchdog disabled.");
    }

    // Non-blocking open for pipes 
    int fd_DBB = open(fifoDBB, O_RDONLY | O_NONBLOCK);
    if (fd_DBB == -1) { endwin(); perror("open read"); exit(1); }
  
    int fd_BBD = open(fifoBBD, O_WRONLY); 
    if (fd_BBD == -1) { endwin(); perror("open write BBDIS"); exit(1); }

    int fd_BBDIS = open(fifoBBDIS, O_WRONLY);
    if (fd_BBDIS == -1) { endwin(); perror("open write BBDIS"); exit(1); }

    int fd_BBObs = open(fifoBBObs, O_WRONLY );
    if (fd_BBObs == -1) { endwin(); perror("open write BBObs"); exit(1); }
  
    int fd_ObsBB = open(fifoObsBB, O_RDONLY); 
    if (fd_ObsBB == -1) { endwin(); perror("open read ObsBB"); exit(1); }

    int fd_BBTar = open(fifoBBTar, O_WRONLY );
    if (fd_BBTar == -1) { endwin(); perror("open write BBTar"); exit(1); }
  
    int fd_TarBB = open(fifoTarBB, O_RDONLY); 
    if (fd_TarBB == -1) { endwin(); perror("open read TarBB"); exit(1); }

    // NETWORK INITIALIZATION & HANDSHAKE 
    if (operation_mode != 0) 
    {
        int is_server = (operation_mode == 1);
        if (network_init(&net_ctx, server_ip, port, is_server) < 0) 
        {
            log_msg("SERVER", "CRITICAL ERROR: Network Init Failed (Check Port/IP)");
            keep_running = 0;
        }

        // Protocol Step 1: Handshake "ok" <-> "ook" 
        if (protocol_handshake(&net_ctx) < 0) 
        {
            log_msg("SERVER", "CRITICAL ERROR: Handshake Failed");
            keep_running = 0;
        }

        // Protocol Step 2: Window Size Exchange 
        int w = MAP_WIDTH, h = MAP_HEIGHT;
        if (protocol_exchange_window_size(&net_ctx, &w, &h) < 0) 
        {
            fprintf(stderr, "Size Exchange Failed\n");
            keep_running = 0;
        }
        // If Client, update our map size to match server
        if (!is_server) 
        {
            // Note: In a real app you might need to resize the window here
            log_msg("NETWORK", "Synced Map Size: %dx%d", w, h);
        }
    }

    // NCURSES INIT
    init_console();

    DroneState incoming_drone_state;
    TargetPacket tar_packet;

    // Position containers for Network Logic
    Point2D local_pos = {0, 0};
    Point2D remote_pos = {0, 0};

    while(keep_running) 
    {
        // Send heartbeat to the watchdog
        if (pid_wd > 0) 
        {
            kill(pid_wd, SIGUSR1);
        }

        // READ INPUT (From Local Drone Controller)
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
                
                // Reset Counters
                targets_spawned_total = 0;
                targets_collected_total = 0;

                // Clear Arrays (Set active = 0)
                for(int i=0; i<MAX_OBSTACLES; i++) world.obstacles[i].active = 0;
                for(int i=0; i<MAX_TARGETS; i++) world.targets[i].active = 0;
                
                // Reset Drone Position visually
                world.drone.x = 10.0;
                world.drone.y = 10.0;

                continue; 
            }

            // In Server/Standalone mode, local input moves the main drone
            if (operation_mode == 0 || operation_mode == 1) 
            {
                world.drone = incoming_drone_state;
            }
            // In Client mode, we just store the input to send to server later
            else if (operation_mode == 2) 
            {
                // Client drone is "local_pos", but main display "world.drone" comes from server
                local_pos.x = incoming_drone_state.x;
                local_pos.y = incoming_drone_state.y;
            }
        }

        // CORE LOGIC
        if (operation_mode == 0) // STANDALONE
        {
            // (Original logic for Obstacles/Targets...)
            write(fd_BBObs, &world.drone, sizeof(DroneState));
            read(fd_ObsBB, world.obstacles, sizeof(world.obstacles));
            
            write(fd_BBTar, &world.drone, sizeof(DroneState));
            read(fd_TarBB, &tar_packet, sizeof(TargetPacket));
            memcpy(world.targets, tar_packet.targets, sizeof(world.targets));
            world.score += tar_packet.score_increment;
        }
        else // NETWORK MODE (SERVER OR CLIENT)
        {
            // Prepare local position for network transmission
            if (operation_mode == 1) 
            { // SERVER
                local_pos.x = world.drone.x;
                local_pos.y = world.drone.y;
            } 
            // Client local_pos was set above in input reading
            // PROTOCOL EXCHANGE
            // This function handles the virtual coordinate conversion internally
            int status = protocol_exchange_positions(&net_ctx, &local_pos, &remote_pos, MAP_HEIGHT);
            
            if (status == -2) // Quit signal received
            { 
                keep_running = 0;
            } 
            else if (status == 0) 
            {
                if (operation_mode == 1) // SERVER
                { 
                    // Remote pos is Client Drone -> Treat as Obstacle 0 
                    world.obstacles[0].x = remote_pos.x;
                    world.obstacles[0].y = remote_pos.y;
                    world.obstacles[0].active = 1;
                } 
                else // CLIENT
                { 
                    // Remote pos is Server Drone -> Update main display
                    world.drone.x = remote_pos.x;
                    world.drone.y = remote_pos.y;
                }
            }
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

    // Network Cleanup
    if (operation_mode != 0) 
    {
        protocol_send_quit(&net_ctx);
        network_close(&net_ctx);
    }

    // Close pipes
    close(fd_DBB);
    close(fd_BBD);
    close(fd_BBDIS);
    close(fd_BBObs);
    close(fd_ObsBB);
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
    unlink(fifoBBObs);
    unlink(fifoObsBB);
    unlink(fifoBBTar);
    unlink(fifoTarBB);

    // Force kill group to ensure terminal windows close
    system("pkill -f drone");
    system("pkill -f keyboard");
    system("pkill -f obstacle_process");
    system("pkill -f target_process");

    // Logging end of the main process to the log file
    log_msg("MAIN", "Clean exit. Bye!");

    return 0;
}