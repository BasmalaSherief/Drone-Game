#ifndef TARGETGENERATOR_H
#define TARGETGENERATOR_H

#include "../common.h"

// CONSTANTS 
#define TARGET_SPAWN_RATE 2   // % chance to spawn a new one per frame
#define COLLECTION_RADIUS 1.0 // How close the drone must be to "eat" it

// Functions
// GENERATOR 
int refresh_targets(Target targets[], const DroneState *drone);

// COLLISION MANAGER 
int check_target_collision(Target targets[], const DroneState *drone);

#endif