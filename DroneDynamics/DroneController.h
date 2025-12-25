#ifndef DRONECONTROLLER_H
#define DRONECONTROLLER_H

#include "../common.h"

/*  ASSIGNMENT1 CORRECTION:
        - Fixed the lag in the drone motion (by changing the parameters)
        now every press moves the drone.
*/

// CONSTANTS 
#define MASS 1       
#define DRAG_COEF 0.8
#define DT 0.03         
#define THRUST_MULTIPLIER 20

// Functions
// PHYSICS ENGINE
void update_physics(DroneState *drone);

#endif