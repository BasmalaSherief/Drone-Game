#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include <fcntl.h>  
#include "DroneController.h"
#include "../common.h"
#include "../ObstaclesGenerator/ObstaclesGenerator.h"

// PHYSICS ENGINE 
void update_physics(DroneState *drone) {
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

int main() {
    // Pipes Setup 
    const char *fifoKD = "/tmp/fifoKD"; // Between Keyboard and Drone Controller
    const char *fifoDBB = "/tmp/fifoDBB"; // Send to the Blackboard Server
    const char *fifoBBD = "/tmp/fifoBBD"; // Recieve from the Blackboard server

    mkfifo(fifoKD, 0666);
    mkfifo(fifoDBB, 0666);
    mkfifo(fifoBBD, 0666);
    
    int fd_KB = open(fifoKD, O_RDONLY);
    int fd_DBB = open(fifoDBB, O_WRONLY);
    int fd_BBD = open(fifoBBD, O_RDONLY);

    // Set Keyboard Pipe to Non-Blocking
    int flags_kb = fcntl(fd_KB, F_GETFL, 0);
    fcntl(fd_KB, F_SETFL, flags_kb | O_NONBLOCK);

    // Set Blackboard Read Pipe to Non-Blocking 
    int flags_bb = fcntl(fd_BBD, F_GETFL, 0);
    fcntl(fd_BBD, F_SETFL, flags_bb | O_NONBLOCK);

    // Initial State
    DroneState drone = { .x = 10.0, .y = 10.0, .vx = 0, .vy = 0, .force_x = 0, .force_y = 0 };
    int game_active = 0; // 0 = IDLE, 1 = FLYING
    Obstacle obstacles[MAX_OBSTACLES];

    while(1) 
    {
        InputMsg msg;
        // Reset inputs every frame
        msg.command = 0; 
        msg.force_x = 0; 
        msg.force_y = 0;

        // Read Input (Non-blocking)
        ssize_t bytesRead = read(fd_KB, &msg, sizeof(msg));
        
        // Read Obstacles (Non-blocking recommended)
        read(fd_BBD, obstacles, sizeof(obstacles));
        
        // HANDLE QUIT 
        if (bytesRead > 0 && msg.command == 'q') 
        {
            drone.x = -1.0; // Use a distinct value to signal termination
            write(fd_DBB, &drone, sizeof(drone));
            break; 
        }

        // HANDLE START/RESET
        if (bytesRead > 0) 
        {
            if (msg.command == 's') game_active = 1;
            else if (msg.command == 'r') 
            {
                DroneState reset_sig = drone;
                reset_sig.x = -2.0; 
                write(fd_DBB, &reset_sig, sizeof(DroneState));
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

        write(fd_DBB, &drone, sizeof(drone));

        usleep(30000); 
    }
    close(fd_KB);
    close(fd_DBB);
    close(fd_BBD);
    return 0;
}