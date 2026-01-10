#ifndef DRONECONTROLLER_H
#define DRONECONTROLLER_H

#include "../common.h"

/*  ASSIGNMENT1 CORRECTION:
        - Fixed the lag in the drone motion (by changing the parameters)
        now every press moves the drone.

        PHYSICS TUNING FOR SMOOTH MOVEMENT:
        - Reduced DRAG_COEF from 0.8 to 0.5 for less resistance
        - Increased DT from 0.03 to 0.05 for smoother integration
        - Reduced THRUST_MULTIPLIER from 20 to 10 for controllable accele
*/

// CONSTANTS 
#define MASS 1       
#define DRAG_COEF 0.5
#define DT 0.05         
#define THRUST_MULTIPLIER 10.0

// Functions
// PHYSICS ENGINE
void update_physics(DroneState *drone);

#endif