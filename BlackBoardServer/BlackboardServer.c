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
    
    // INITIALIZE NETWORK
    net_config = init_network_config("param.conf");

    // The next code block is from the assigment1 fixes
    // LAUNCH CHILDREN 
    // Launch Drone
    char *arg_list_drone[] = { "./drone", NULL };
    pid_drone = spawn_process("./drone", arg_list_drone);
    log_msg("MAIN", "Launched Drone with PID: %d", pid_drone);

    // Launch Keyboard
    // pass the keyboard executable as an argument (konsole -e ./keyboard)
    char *arg_list_kb[] = { "konsole", "-e", "./keyboard", NULL };
    pid_keyboard = spawn_process("konsole", arg_list_kb);
    log_msg("MAIN", "Launched Keyboard Manager with PID: %d", pid_keyboard);

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

    log_msg("MAIN", "Opening pipes...");
    // OPEN CORE PIPES (Used in ALL modes)
    int fd_DBB = open(fifoDBB, O_RDONLY | O_NONBLOCK);
    if (fd_DBB == -1) { perror("open read fifoDBB"); exit(1); }
  
    int fd_BBD = open(fifoBBD, O_WRONLY); 
    if (fd_BBD == -1) { perror("open write fifoBBD"); exit(1); }

    int fd_BBDIS = open(fifoBBDIS, O_WRONLY);
    if (fd_BBDIS == -1) { perror("open write fifoBBDIS"); exit(1); }

    // OPEN GENERATOR PIPES (Only in STANDALONE mode)
    // Initialize to -1 so we don't accidentally use them in Network mode
    int fd_BBObs = -1;
    int fd_ObsBB = -1;
    int fd_BBTar = -1;
    int fd_TarBB = -1;

    if (net_config->mode == MODE_STANDALONE)
    {
        fd_BBObs = open(fifoBBObs, O_WRONLY);
        if (fd_BBObs == -1) { perror("open write fifoBBObs"); exit(1); }
    
        fd_ObsBB = open(fifoObsBB, O_RDONLY); 
        if (fd_ObsBB == -1) { perror("open read fifoObsBB"); exit(1); }

        fd_BBTar = open(fifoBBTar, O_WRONLY);
        if (fd_BBTar == -1) { perror("open write fifoBBTar"); exit(1); }
    
        fd_TarBB = open(fifoTarBB, O_RDONLY); 
        if (fd_TarBB == -1) { perror("open read fifoTarBB"); exit(1); }
    }

    log_msg("MAIN", "Pipes opened successfully.");

    if (net_config->mode == MODE_SERVER) 
    {
        log_msg("SERVER", "Starting in SERVER mode");
        // Initialize network
        if (network_server_init(net_config) < 0) 
        {
            fprintf(stderr, "Failed to initialize server\n");
            exit(1);
        }
        server_handshake(net_config->socket_fd);
        server_send_size(net_config->socket_fd, MAP_WIDTH, MAP_HEIGHT);
    }
    
    else if (net_config->mode == MODE_CLIENT) 
    {
        log_msg("CLIENT", "Starting in CLIENT mode");
        // Connect to server
        if (network_client_init(net_config) < 0) 
        {
            fprintf(stderr, "Failed to connect to server\n");
            exit(1);
        }
        client_handshake(net_config->socket_fd);
        int width, height;
        client_receive_size(net_config->socket_fd, &width, &height);
        // Adjust window size to match server
        log_msg("CLIENT", "Server window size: %dx%d", width, height);
    }

    // NCURSES INIT
    init_console();

    DroneState incoming_drone_state;
    TargetPacket tar_packet;

    // Variable to track the LOCAL drone position (for Client mode)
    DroneState local_drone = world.drone;

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
            // Send Main Drone Pos to Client
            if (server_send_drone(net_config->socket_fd, world.drone.x, world.drone.y) < 0) 
            {
                log_msg("SERVER", "Network Error sending drone");
                keep_running = 0;
            }

            // Receive Client's Drone (as an Obstacle)
            float ox, oy;
            if (server_receive_obstacle(net_config->socket_fd, &ox, &oy) < 0) 
            {
                log_msg("SERVER", "Network Error receiving obstacle");
                keep_running = 0;
            } 
            else 
            {
                // Update Obstacle 0 to match Client position
                world.obstacles[0].x = (int)ox;
                world.obstacles[0].y = (int)oy;
                world.obstacles[0].active = 1; // Make it visible
            }
        }

        // --- CLIENT MODE ---
        else if (net_config->mode == MODE_CLIENT && net_config->connected)
        {
            // Receive Main Drone Pos from Server (and update world)
            float sx, sy;
            if (client_receive_drone(net_config->socket_fd, &sx, &sy) < 0) 
            {
                log_msg("CLIENT", "Network Error receiving drone");
                keep_running = 0;
            } 
            else 
            {
                world.drone.x = sx;
                world.drone.y = sy;
            }

            // Send Local Drone (our input) to Server (as obstacle)
            if (client_send_obstacle(net_config->socket_fd, local_drone.x, local_drone.y) < 0) 
            {
                log_msg("CLIENT", "Network Error sending obstacle");
                keep_running = 0;
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