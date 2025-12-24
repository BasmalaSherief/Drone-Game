#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include <fcntl.h>  
#include <errno.h>
#include <signal.h>
#include "DroneController.h"
#include "../common.h"
#include "../ObstaclesGenerator/ObstaclesGenerator.h"

// GLOBAL FLAG FOR CLEANUP
volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) 
{
    keep_running = 0;
}

// PHYSICS ENGINE 
void update_physics(DroneState *drone) 
{
    // Add Drag Force (Air Resistance), Drag always opposes velocity: F_drag = -K * v
    drone->force_x -= DRAG_COEF * drone->vx;
    drone->force_y -= DRAG_COEF * drone->vy;

    // Calculate Acceleration (a = F / m)
    double ax = drone->force_x / MASS;
    double ay = drone->force_y / MASS;

    // Integration (Euler Method: NewValue = OldValue + (RateOfChange × Δt) )
    // Update Velocity
    drone->vx += ax * DT;
    drone->vy += ay * DT;

    // Update Position
    drone->x += drone->vx * DT;
    drone->y += drone->vy * DT;
}

int main() 
{
    // REGISTER SIGNALS
    signal(SIGINT, handle_signal);  // Ctrl+C
    signal(SIGTERM, handle_signal); // Kill command

    static int frame_count = 0;

    // Pipes Setup 
    const char *fifoKD = "/tmp/fifoKD"; // Between Keyboard and Drone Controller
    const char *fifoDBB = "/tmp/fifoDBB"; // Send to the Blackboard Server
    const char *fifoBBD = "/tmp/fifoBBD"; // Recieve from the Blackboard server

    if (mkfifo(fifoKD, 0666) == -1 && errno != EEXIST) { perror("Drone fifoKD"); exit(1); }
    if (mkfifo(fifoDBB, 0666) == -1 && errno != EEXIST) { perror("Drone fifoDBB"); exit(1); }
    if (mkfifo(fifoBBD, 0666) == -1 && errno != EEXIST) { perror("Drone fifoBBD"); exit(1); }
    
    int fd_KD = open(fifoKD, O_RDONLY);
    int fd_DBB = open(fifoDBB, O_WRONLY);
    int fd_BBD = open(fifoBBD, O_RDONLY);

    if (fd_KD == -1) { perror("Pipe From Keyboard to Drone: open read"); exit(1); }
    if (fd_DBB == -1) { perror("Pipe From Drone to BlackBoard: open write"); exit(1); }
    if (fd_BBD == -1)  { perror("Pipe From BlackBoard to Drone: open read"); exit(1); }

    // Set Keyboard Pipe to Non-Blocking
    fcntl(fd_KD, F_SETFL, fcntl(fd_KD, F_GETFL, 0) | O_NONBLOCK);

    // Set Blackboard Read Pipe to Non-Blocking 
    fcntl(fd_BBD, F_SETFL, fcntl(fd_BBD, F_GETFL, 0) | O_NONBLOCK);

    // Initial State
    DroneState drone = { .x = 10.0, .y = 10.0, .vx = 0, .vy = 0, .force_x = 0, .force_y = 0 };
    int game_active = 0; // 0 = IDLE, 1 = FLYING
    Obstacle obstacles[MAX_OBSTACLES];

    while(keep_running) 
    {
        InputMsg msg = {0,0,0};
        // Reset inputs every frame
        msg.command = 0; 
        msg.force_x = 0; 
        msg.force_y = 0;

        // Read Input (Non-blocking)
        ssize_t bytesRead = read(fd_KD, &msg, sizeof(msg));
        
        // Read Obstacles (Non-blocking)
        ssize_t obsBytes = read(fd_BBD, obstacles, sizeof(obstacles));
        if (obsBytes == -1) 
        {
            if (errno != EAGAIN) 
            {
                perror("Drone: Error reading obstacles");
            }
        } 
        else if (obsBytes > 0 && obsBytes < sizeof(obstacles)) 
        {
            fprintf(stderr, "Drone: Warning - Partial obstacle data received.\n");
        }
        
        // HANDLE QUIT 
        /*  ASSIGNMENT1 CORRECTION:
                - fixing the killing 
        */
        if (bytesRead == 0) 
        {
            // Keyboard process has died (EOF). We should quit too.
            printf("Drone: Keyboard disconnected. Stopping.\n");
            msg.command = 'q'; // Force a quit command
            bytesRead = 1;     // Pretend we read data so the quit logic below triggers
        }
        else if (bytesRead == -1) 
        {
            if (errno != EAGAIN) { perror("Drone: Error reading input commands"); }
        }
        
        if (bytesRead > 0 && msg.command == 'q') 
        {
            drone.x = -1.0; // A distinct value to signal termination           
            ssize_t quitBytes = write(fd_DBB, &drone, sizeof(drone));
            if (quitBytes == -1) perror("Drone: Failed to send reset signal");
            keep_running = 0; // Break loop gracefully 
        }

        // HANDLE START/RESET
        if (bytesRead > 0) 
        {
            if (msg.command == 's') game_active = 1;
            else if (msg.command == 'r') 
            {
                DroneState reset_sig = drone;
                reset_sig.x = -2.0; 
                ssize_t resetBytes = write(fd_DBB, &reset_sig, sizeof(DroneState));
                if (resetBytes == -1) perror("Drone: Failed to send reset signal");
                drone.x = 10.0; drone.y = 10.0;
                drone.vx = 0.0; drone.vy = 0.0;
                drone.force_x = 0.0; drone.force_y = 0.0;
                game_active = 0; 
            }
        } 

        // PHYSICS LOOP
        if (game_active) {
            drone.force_x = 0; 
            drone.force_y = 0;

            // Add Input Forces
            drone.force_x += msg.force_x * THRUST_MULTIPLIER;
            drone.force_y += msg.force_y * THRUST_MULTIPLIER;

            // Brake
            if (msg.command == ' ') 
            {
                drone.vx *= 0.5;
                drone.vy *= 0.5;
            }

            // Repulsion & Integration
            apply_repulsive_forces(&drone, obstacles);
            apply_border_forces(&drone);
            update_physics(&drone);
        }

        // SEND STATE TO BLACKBOARD
        ssize_t stateBytes = write(fd_DBB, &drone, sizeof(drone));
        if (stateBytes == -1) 
        {
            perror("Drone: Error sending state to Blackboard");
            // If the server is dead (EPIPE), we might want to quit the drone too.
            if (errno == EPIPE) break; 
        }

        // Logging drone Data to the log file only every 100 frames
        if (frame_count++ % 100 == 0) 
        {
            log_msg("PHYSICS", "Pos: (%.2f, %.2f), Vel: (%.2f, %.2f)", drone.x, drone.y, drone.vx, drone.vy);
        }

        usleep(30000); 
    }
    close(fd_KD);
    close(fd_DBB);
    close(fd_BBD);
    log_msg("DRONE", "Exiting cleanly");
    return 0;
}