#include <stdlib.h> 
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>  
#include "../common.h" 
#include "ObstaclesGenerator.h"

// Flags
volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) 
{
    keep_running = 0;
}

int main(int argc, char *argv[]) 
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN); // Prevent crash if Server dies

    srand(time(NULL) + getpid()); // Unique seed

    // PIPES
    // Read Drone State from Server 
    const char *fifoBBObs = "/tmp/fifoBBObs"; 
    // Write Obstacle Array to Server 
    const char *fifoObsBB = "/tmp/fifoObsBB";
    
    // Open the reading Pipe 
    int fd_BBObs = open(fifoBBObs, O_RDONLY);
    if (fd_BBObs == -1) { perror("ObsProcess: open Req"); return 1; }

    // Open the writing Pipe 
    int fd_ObsBB = open(fifoObsBB, O_WRONLY);
    if (fd_ObsBB == -1) { perror("ObsProcess: open Data"); return 1; }

    // Local Data
    DroneState drone = {0};
    Obstacle obstacles[MAX_OBSTACLES];
    // Init obstacles
    for(int i=0; i<MAX_OBSTACLES; i++) obstacles[i].active = 0;

    while(keep_running) 
    {
        // Wait for Drone State from Server 
        ssize_t bytes = read(fd_BBObs, &drone, sizeof(DroneState));
        if (bytes <= 0) break; // Server closed

        // Run Lifecycle Logic (Spawn/Despawn/Timers)
        // This function is defined in Obstacles_functions.c
        update_obstacle_lifecycle(obstacles, &drone);

        // Send Updated Array back to Server
        write(fd_ObsBB, obstacles, sizeof(obstacles));
    }
    
    // Cleanup
    close(fd_BBObs);
    close(fd_ObsBB);
    return 0;
    
}