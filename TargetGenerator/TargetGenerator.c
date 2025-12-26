#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include "../common.h"
#include "TargetGenerator.h"

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
    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL) + getpid()); // Unique random seed

    // PIPES 
    // Read Drone State from Server 
    const char *fifoBBTar = "/tmp/fifoBBTar";   
    // Write Targets + Score to the server
    const char *fifoTarBB = "/tmp/fifoTarBB"; 

    int fd_BBTar = open(fifoBBTar, O_RDONLY);
    if (fd_BBTar == -1) { perror("TargetProc: open Req"); return 1; }

    int fd_TarBB = open(fifoTarBB, O_WRONLY);
    if (fd_TarBB == -1) { perror("TargetProc: open Data"); return 1; }

    // LOCAL STATE
    DroneState drone = {0};
    Target targets[MAX_TARGETS];
    
    // Initialize targets to inactive
    for(int i=0; i<MAX_TARGETS; i++) targets[i].active = 0;

    // Counter for the total targets generated
    int targets_spawned_total = 0;

    TargetPacket packet;

    while(keep_running) 
    {
        // Wait for Drone State from Server
        ssize_t bytes = read(fd_BBTar, &drone, sizeof(DroneState));
        if (bytes <= 0) break; // Server closed connection
        
        // Check Collisions ( If drone touches target -> active=0, return score)
        int score = check_target_collision(targets, &drone);

        // Spawn logic
        if (targets_spawned_total < TOTAL_TARGETS_TO_WIN) 
        {
            // Call the refresh function
            int spawned = refresh_targets(targets, &drone);

            // Execute this logic if a new target was actually created
            if (spawned > 0) 
            {
                targets_spawned_total += spawned;

                // Log the new target position by finding an active one
                for(int i=0; i<MAX_TARGETS; i++) 
                {
                    if (targets[i].active) 
                    {
                        // Log the newly spawned target
                        log_msg("TARGET_PROC", "Spawned new target at %d,%d (Total: %d)", 
                                targets[i].x, targets[i].y, targets_spawned_total);
                        break; 
                    }
                }
            }
        }

        // Prepare Data Packet
        // Copy our local targets to the packet
        memcpy(packet.targets, targets, sizeof(targets));
        packet.score_increment = score;

        // Send back to Server
        write(fd_TarBB, &packet, sizeof(TargetPacket));
    }

    // Cleanup
    close(fd_BBTar);
    close(fd_TarBB);
    return 0;
}