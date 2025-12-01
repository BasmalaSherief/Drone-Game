#ifndef DRONECONTROLLER_H
#define DRONECONTROLLER_H

#include "../common.h"

// CONSTANTS 
#define MASS 1       
#define DRAG_COEF 0.8
#define DT 0.1         
#define THRUST_MULTIPLIER 2.0 

// Functions
// PHYSICS ENGINE
void update_physics(DroneState *drone);

#endif