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
#include "../TargetGenerator/TargetGenerator.h"
#include "../ObstaclesGenerator/ObstaclesGenerator.h"

/*  ASSIGNMENT1 CORRECTION:
        - fixed the killing through the handle_signal and global flag for cleaning
        which is found in the drone and keyboard files too 
        - Added error checking when dealing (making, opening, reading, writing) with pipes
        and 
*/

// GLOBAL FLAG FOR CLEANUP
volatile sig_atomic_t keep_running = 1;

// Global PIDs to track children
pid_t pid_drone;
pid_t pid_keyboard;

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

    if (mkfifo(fifoDBB, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoDBB"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoBBD, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBD"); exit(EXIT_FAILURE); }
    if (mkfifo(fifoBBDIS, 0666) == -1 && errno != EEXIST) { perror("Server: Failed to create fifoBBDIS"); exit(EXIT_FAILURE); }

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

    // NCURSES INIT
    init_console();

    // Non-blocking open for pipes 
    int fd_DBB = open(fifoDBB, O_RDONLY | O_NONBLOCK);
    if (fd_DBB == -1) { endwin(); perror("open read"); exit(1); }
  
    int fd_BBD = open(fifoBBD, O_WRONLY); 
    if (fd_BBD == -1) { endwin(); perror("open write BBDIS"); exit(1); }

    int fd_BBDIS = open(fifoBBDIS, O_WRONLY);
    if (fd_BBDIS == -1) { endwin(); perror("open write BBDIS"); exit(1); }

    DroneState incoming_drone_state;

    while(keep_running) 
    {
        // READ INPUT 
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
                world.drone = incoming_drone_state;
                world.game_active = 1;                
            }

        }

        // LOGIC 
        if (world.game_active) 
        {
            // Obstacles 
            update_obstacle_lifecycle(world.obstacles, &world.drone);

            // Targets (Spawn Logic)
            if (targets_spawned_total < TOTAL_TARGETS_TO_WIN) 
            {
                int spawned = refresh_targets(world.targets, &world.drone);
                
                // Execute this logic if a NEW target was actually created
                if (spawned > 0) 
                {
                    targets_spawned_total += spawned;

                    // Log the new target position by finding the active one
                    for(int i=0; i<MAX_TARGETS; i++) {
                        if (world.targets[i].active) {
                             // Log the targets
                             log_msg("SERVER", "Received new target at %d,%d", world.targets[i].x, world.targets[i].y);
                             break; 
                        }
                    }
                }
            }

            // Targets (Collision Logic)
            int points = check_target_collision(world.targets, &world.drone);
            if(points > 0) 
            {
                world.score += points;
                targets_collected_total += points;
                log_msg("SERVER", "Target collected. Score: %d", world.score);
            }

            // CHECK WIN CONDITION
            if (targets_collected_total >= TOTAL_TARGETS_TO_WIN) 
            {
                erase();
                attron(A_BOLD | COLOR_PAIR(COLOR_TARGET));
                mvprintw(MAP_HEIGHT/2, MAP_WIDTH/2 - 5, "YOU WIN!");
                mvprintw(MAP_HEIGHT/2 + 1, MAP_WIDTH/2 - 10, "Final Score: %d", world.score);
                attroff(A_BOLD | COLOR_PAIR(COLOR_TARGET));
                refresh();
                sleep(4);  
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
                printf("Server: Drone disconnected. Cannot write obstacles.\n");
            } 
            else 
            {
                perror("Server: Error writing to Drone Pipe (fifoBBD)");
            }
        }

        // WRITING TO KEYBOARD DISPLAY
        ssize_t bytesWrittenBBDIS = write(fd_BBDIS, &world, sizeof(WorldState));
        if (bytesWrittenBBDIS == -1) 
        {
            if (errno == EPIPE) 
            {
                printf("Server: Keyboard/Display disconnected. Cannot update UI.\n");
            } 
            else if (errno != EAGAIN) 
            { 
                perror("Server: Error writing to Display Pipe (fifoBBDIS)");
            }
        }

        usleep(30000); 
    }

    // CLEANUP

    log_msg("MAIN", "Stopping system...");

    // Kill children using their PIDs
    if (pid_drone > 0) kill(pid_drone, SIGTERM);
    if (pid_keyboard > 0) kill(pid_keyboard, SIGTERM);
    
    // Wait for them to finish to avoid zombies
    waitpid(pid_drone, NULL, 0);
    waitpid(pid_keyboard, NULL, 0);

    // Destroy Ncurses window
    endwin();  

    // Close pipes
    close(fd_DBB);
    close(fd_BBD);
    close(fd_BBDIS);

    // Unlink pipes so they don't persist
    unlink(fifoDBB);
    unlink(fifoBBD);
    unlink(fifoBBDIS);

    // Force kill group to ensure terminal windows close
    system("pkill -f drone");
    system("pkill -f keyboard");

    // Logging end of the main process to the log file
    log_msg("MAIN", "Clean exit. Bye!");

    return 0;
}