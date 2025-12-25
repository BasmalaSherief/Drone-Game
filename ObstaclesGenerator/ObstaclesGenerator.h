#ifndef OBSTACLESGENERATOR_H
#define OBSTACLESGENERATOR_H

#include "../common.h" 

/*  ASSIGNMENT1 CORRECTION:
        - Fixed the repulsive forces of obtacles and borders (by changing the parameters).
*/

// Lifecycle Constants
#define OBSTACLE_LIFETIME 500 // Frames the obstacle stays alive
#define SPAWN_CHANCE 5        // 5% chance per frame to spawn a new one
#define SAFE_RADIUS 8.0       // Don't spawn within 8 units of the drone

// Physics Constants
#define REPULSIVE_GAIN 50   // Strength of the push
#define INFLUENCE_RANGE 4.0   // Distance at which the force starts working
#define MAX_FORCE 20        // Safety cap to prevent physics glitches
#define BORDER_MARGIN_SPAWN 5
#define BORDER_MARGIN 2.0      // Start pushing 2 units away from wall
#define BORDER_GAIN 100       // How strong the wall pushes

// Functions
// GENERATOR (Lifecycle Logic) 
void update_obstacle_lifecycle(Obstacle obstacles[], DroneState *drone);

// PHYSICS (Repulsive Logic) 
void apply_repulsive_forces(DroneState *drone, Obstacle obstacles[]); 
void apply_border_forces(DroneState *drone);

#endif