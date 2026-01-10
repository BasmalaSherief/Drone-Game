#ifndef OBSTACLESGENERATOR_H
#define OBSTACLESGENERATOR_H

#include "../common.h" 

/*  ASSIGNMENT1 CORRECTION:
        - Fixed the repulsive forces of obstacles and borders (by changing the parameters)
    
    ASSIGNMENT2 FIX - "STRONG REPULSION FROM EDGES":
        - BORDER_GAIN reduced from 50 to 5 (10x weaker!)
        - BORDER_MARGIN increased from 2.0 to 4.0 (earlier, gentler warning)
        - REPULSIVE_GAIN reduced from 50 to 30 (obstacles less violent)
        - INFLUENCE_RANGE increased from 4.0 to 5.0 (smoother force gradient)
*/

// Lifecycle Constants
#define OBSTACLE_LIFETIME 500 // Frames the obstacle stays alive
#define SPAWN_CHANCE 5        // 5% chance per frame to spawn a new one
#define SAFE_RADIUS 8.0       // Don't spawn within 8 units of the drone

// Physics Constants
#define REPULSIVE_GAIN 15   // Strength of the push
#define INFLUENCE_RANGE 3.0   // Distance at which the force starts working
#define MAX_FORCE 15        // Safety cap to prevent physics glitches
#define BORDER_MARGIN_SPAWN 5 // Border margin for spawning (don't spawn too close to edges)
#define BORDER_MARGIN 4.0      // Start pushing 2 units away from wall
#define BORDER_GAIN 5       // How strong the wall pushes

// Functions
// GENERATOR (Lifecycle Logic) 
void update_obstacle_lifecycle(Obstacle obstacles[], DroneState *drone);

// PHYSICS (Repulsive Logic) 
void apply_repulsive_forces(DroneState *drone, Obstacle obstacles[]); 
void apply_border_forces(DroneState *drone);

#endif