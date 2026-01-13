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

// Global network config
NetworkConfig *net_config = NULL;

// Global PIDs to track children
pid_t pid_drone = -1;
pid_t pid_keyboard = -1;
pid_t pid_obst = -1;
pid_t pid_targ = -1;
pid_t pid_wd = -1;

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

    // RUN STARTUP PROMPT
    prompt_for_mode();

    // Logging start of the main process to the log file
    log_msg("MAIN", "Process started with PID %d", getpid());
    srand(time(NULL));
        
    // INITIALIZE NETWORK
    net_config = init_network_config("param.conf");

    if (!net_config) 
    {
        fprintf(stderr, "Failed to load network config\n");
        exit(1);
    }
    
    log_msg("MAIN", "Running in mode: %d", net_config->mode);

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

    if (net_config->mode == MODE_SERVER) 
    {
        log_msg("SERVER", "Starting in SERVER mode - waiting for client...");
        printf("\n[SERVER] Waiting for client connection on port %d...\n", net_config->port);
        
        if (network_server_init(net_config) < 0) 
        {
            fprintf(stderr, "Failed to initialize server\n");
            exit(1);
        }
        
        printf("[SERVER] Client connected! Starting handshake...\n");
        
        if (server_handshake(net_config->socket_fd) < 0) 
        {
            fprintf(stderr, "Handshake failed\n");
            exit(1);
        }
        
        if (server_send_size(net_config->socket_fd, MAP_WIDTH, MAP_HEIGHT) < 0) 
        {
            fprintf(stderr, "Failed to send size\n");
            exit(1);
        }
        
        log_msg("SERVER", "Network setup complete");
        printf("[SERVER] Setup complete! Starting simulation...\n");
        sleep(1);
    }
    else if (net_config->mode == MODE_CLIENT) 
    {
        log_msg("CLIENT", "Starting in CLIENT mode");
        printf("\n[CLIENT] Connecting to %s:%d...\n", net_config->server_ip, net_config->port);
        
        if (network_client_init(net_config) < 0) 
        {
            fprintf(stderr, "Failed to connect to server\n");
            exit(1);
        }
        
        printf("[CLIENT] Connected! Starting handshake...\n");
        
        if (client_handshake(net_config->socket_fd) < 0) 
        {
            fprintf(stderr, "Handshake failed\n");
            exit(1);
        }
        
        int width, height;
        if (client_receive_size(net_config->socket_fd, &width, &height) < 0) 
        {
            fprintf(stderr, "Failed to receive size\n");
            exit(1);
        }
        
        log_msg("CLIENT", "Server window size: %dx%d", width, height);
        printf("[CLIENT] Setup complete! Starting simulation...\n");
        sleep(1);
    }

    // Create pipes based on mode
    const char *fifoDBB = "/tmp/fifoDBB";
    const char *fifoBBD = "/tmp/fifoBBD";
    const char *fifoBBDIS = "/tmp/fifoBBDIS";

    // Always need these pipes
    if (mkfifo(fifoDBB, 0666) == -1 && errno != EEXIST) 
    { 
        perror("mkfifo fifoDBB"); 
        exit(1); 
    }
    if (mkfifo(fifoBBD, 0666) == -1 && errno != EEXIST) 
    { 
        perror("mkfifo fifoBBD"); 
        exit(1); 
    }
    if (mkfifo(fifoBBDIS, 0666) == -1 && errno != EEXIST) 
    { 
        perror("mkfifo fifoBBDIS"); 
        exit(1); 
    }


    // Only create generator pipes in STANDALONE mode
    const char *fifoBBObs = "/tmp/fifoBBObs";
    const char *fifoObsBB = "/tmp/fifoObsBB";
    const char *fifoBBTar = "/tmp/fifoBBTar";
    const char *fifoTarBB = "/tmp/fifoTarBB";
    
    if (net_config->mode == MODE_STANDALONE) 
    {
        if (mkfifo(fifoBBObs, 0666) == -1 && errno != EEXIST) 
        { 
            perror("mkfifo fifoBBObs"); 
            exit(1); 
        }
        if (mkfifo(fifoObsBB, 0666) == -1 && errno != EEXIST) 
        { 
            perror("mkfifo fifoObsBB"); 
            exit(1); 
        }
        if (mkfifo(fifoBBTar, 0666) == -1 && errno != EEXIST) 
        { 
            perror("mkfifo fifoBBTar"); 
            exit(1); 
        }
        if (mkfifo(fifoTarBB, 0666) == -1 && errno != EEXIST) 
        { 
            perror("mkfifo fifoTarBB"); 
            exit(1); 
        }
    }

    // The next code block is from the assigment1 fixes
    // LAUNCH CHILDREN 
    log_msg("MAIN", "Spawning child processes...");
    // Launch Drone
    char *arg_list_drone[] = { "./drone", NULL };
    pid_drone = spawn_process("./drone", arg_list_drone);
    log_msg("MAIN", "Launched Drone with PID: %d", pid_drone);
    usleep(100000); // 100ms delay

    // Launch Keyboard
    // pass the keyboard executable as an argument (konsole -e ./keyboard)
    char *arg_list_kb[] = { "konsole", "-e", "./keyboard", NULL };
    pid_keyboard = spawn_process("konsole", arg_list_kb);
    log_msg("MAIN", "Launched Keyboard Manager with PID: %d", pid_keyboard);
    usleep(100000); // 100ms delay

    // ONLY launch Generators and Watchdog if STANDALONE
    if (net_config->mode == MODE_STANDALONE)
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

    // NCURSES INIT
    init_console();

    // Open pipes - Always need these three
    int fd_DBB = open(fifoDBB, O_RDONLY | O_NONBLOCK);
    if (fd_DBB == -1) 
    { 
        endwin(); 
        perror("open fifoDBB"); 
        exit(1); 
    }
  
    int fd_BBD = open(fifoBBD, O_WRONLY); 
    if (fd_BBD == -1) 
    { 
        endwin(); 
        perror("open fifoBBD"); 
        exit(1); 
    }

    int fd_BBDIS = open(fifoBBDIS, O_WRONLY);
    if (fd_BBDIS == -1) 
    { 
        endwin(); 
        perror("open fifoBBDIS"); 
        exit(1); 
    }

        // FIX #6: Only open generator pipes in standalone mode
    int fd_BBObs = -1, fd_ObsBB = -1, fd_BBTar = -1, fd_TarBB = -1;
    
    if (net_config->mode == MODE_STANDALONE) 
    {
        fd_BBObs = open(fifoBBObs, O_WRONLY);
        if (fd_BBObs == -1) 
        { 
            endwin(); 
            perror("open fifoBBObs"); 
            exit(1); 
        }
      
        fd_ObsBB = open(fifoObsBB, O_RDONLY); 
        if (fd_ObsBB == -1) 
        { 
            endwin(); 
            perror("open fifoObsBB"); 
            exit(1); 
        }

        fd_BBTar = open(fifoBBTar, O_WRONLY);
        if (fd_BBTar == -1) 
        { 
            endwin(); 
            perror("open fifoBBTar"); 
            exit(1); 
        }
      
        fd_TarBB = open(fifoTarBB, O_RDONLY); 
        if (fd_TarBB == -1) 
        { 
            endwin(); 
            perror("open fifoTarBB"); 
            exit(1); 
        }
    }

    log_msg("MAIN", "All pipes opened successfully");


    DroneState incoming_drone_state;
    TargetPacket tar_packet;
    // Variable to track the LOCAL drone position (for Client mode)
    DroneState local_drone = world.drone;

    while(keep_running) 
    {
        // Send heartbeat to the watchdog in the standalone mode 
        if (net_config->mode == MODE_STANDALONE && pid_wd > 0) 
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
            else
            {
                // If Client: Store input in local_drone (to send to server)
                if (net_config->mode == MODE_CLIENT)
                {
                    local_drone = incoming_drone_state;
                    world.game_active = 1; // Client is active
                }
                // If Server/Standalone: Input controls the main world drone directly.
                else
                {
                    world.drone = incoming_drone_state;
                    world.game_active = 1;
                }              
            }
        }

        // CORE LOGIC BRANCHING
        // --- STANDALONE ---
        if (net_config->mode == MODE_STANDALONE &&world.game_active) 
        {
            // OBSTACLE SYNC
            // Send Drone position to Obstacle Process
            write(fd_BBObs, &world.drone, sizeof(DroneState));
            // Read back updated Obstacles
            read(fd_ObsBB, world.obstacles, sizeof(world.obstacles));

            // TARGET SYNC
            // Send Drone position to Target Process
            write(fd_BBTar, &world.drone, sizeof(DroneState));
            // Read back updated Targets + Score
            read(fd_TarBB, &tar_packet, sizeof(TargetPacket));
            
            // Update local world
            memcpy(world.targets, tar_packet.targets, sizeof(world.targets));
            
            if (tar_packet.score_increment > 0) 
            {
                world.score += tar_packet.score_increment;
                log_msg("SERVER", "Score Update! New Score: %d", world.score);
            }

            // Win Condition
            if (world.score >= TOTAL_TARGETS_TO_WIN) 
            {
                erase();
                attron(A_BOLD | COLOR_PAIR(COLOR_TARGET));
                mvprintw(LINES/2, COLS/2 - 5, "YOU WIN!");
                mvprintw(LINES/2 + 1, COLS/2 - 10, "Final Score: %d", world.score);
                attroff(A_BOLD | COLOR_PAIR(COLOR_TARGET));
                refresh();
                sleep(4);  
                keep_running = 0;
            }
        }
        // --- SERVER MODE ---
        else if (net_config->mode == MODE_SERVER && net_config->connected)
        {
            // Server sends its drone, receives client's as obstacle
            if (server_send_drone(net_config->socket_fd, world.drone.x, world.drone.y) < 0) 
            {
                log_msg("SERVER", "Network error - client disconnected");
                keep_running = 0;
                continue;
            }

            float ox, oy;
            if (server_receive_obstacle(net_config->socket_fd, &ox, &oy) < 0) 
            {
                log_msg("SERVER", "Network error - client disconnected");
                keep_running = 0;
                continue;
            }
            
            // Update obstacle slot 0 with client position
            world.obstacles[0].x = (int)ox;
            world.obstacles[0].y = (int)oy;
            world.obstacles[0].active = 1;
        }

        // --- CLIENT MODE ---
        else if (net_config->mode == MODE_CLIENT && net_config->connected)
        {
            // Client receives server's drone, sends own as obstacle
            float sx, sy;
            if (client_receive_drone(net_config->socket_fd, &sx, &sy) < 0) 
            {
                log_msg("CLIENT", "Network error - server disconnected");
                keep_running = 0;
                continue;
            }
            
            world.drone.x = sx;
            world.drone.y = sy;

            if (client_send_obstacle(net_config->socket_fd, local_drone.x, local_drone.y) < 0) 
            {
                log_msg("CLIENT", "Network error - server disconnected");
                keep_running = 0;
                continue;
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
        ssize_t bytesWritten = write(fd_BBD, world.obstacles, sizeof(world.obstacles));
        if (bytesWritten == -1 && errno == EPIPE) 
        {
            log_msg("SERVER", "Drone disconnected");
            keep_running = 0;
        }

        bytesWritten = write(fd_BBDIS, &world, sizeof(WorldState));
        if (bytesWritten == -1 && errno == EPIPE) 
        {
            log_msg("SERVER", "Keyboard disconnected");
            keep_running = 0;
        }

        usleep(30000); 
    }

    // CLEANUP

    log_msg("MAIN", "Stopping system...");

    // Send network quit signal
    if (net_config->connected && net_config->socket_fd != -1) 
    {
        send_quit(net_config->socket_fd);
        close(net_config->socket_fd);
    }

    // Close pipes
    if (fd_DBB != -1) close(fd_DBB);
    if (fd_BBD != -1) close(fd_BBD);
    if (fd_BBDIS != -1) close(fd_BBDIS);
    
    if (net_config->mode == MODE_STANDALONE) 
    {
        if (fd_BBObs != -1) close(fd_BBObs);
        if (fd_ObsBB != -1) close(fd_ObsBB);
        if (fd_BBTar != -1) close(fd_BBTar);
        if (fd_TarBB != -1) close(fd_TarBB);
    }

    // Terminate children
    if (pid_drone > 0) kill(pid_drone, SIGTERM);
    if (pid_keyboard > 0) kill(pid_keyboard, SIGTERM);
    if (pid_obst > 0) kill(pid_obst, SIGTERM);
    if (pid_targ > 0) kill(pid_targ, SIGTERM);
    if (pid_wd > 0) kill(pid_wd, SIGTERM);
    
    // Wait for cleanup
    if (pid_drone > 0) waitpid(pid_drone, NULL, WNOHANG);
    if (pid_keyboard > 0) waitpid(pid_keyboard, NULL, WNOHANG);
    if (pid_obst > 0) waitpid(pid_obst, NULL, WNOHANG);
    if (pid_targ > 0) waitpid(pid_targ, NULL, WNOHANG);
    if (pid_wd > 0) waitpid(pid_wd, NULL, WNOHANG);

    endwin();

    // Cleanup pipes
    unlink(fifoDBB);
    unlink(fifoBBD);
    unlink(fifoBBDIS);
    
    if (net_config->mode == MODE_STANDALONE) 
    {
        unlink(fifoBBObs);
        unlink(fifoObsBB);
        unlink(fifoBBTar);
        unlink(fifoTarBB);
    }

    if (net_config) free(net_config);
    
    log_msg("MAIN", "Clean exit");
    return 0;
}