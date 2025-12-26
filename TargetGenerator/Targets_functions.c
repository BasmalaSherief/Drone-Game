#include <stdlib.h>
#include <math.h>
#include "../common.h"
#include "TargetGenerator.h"

// GENERATOR 
int refresh_targets(Target targets[], const DroneState *drone) 
{   
    int active_count = 0;
    for (int i = 0; i < MAX_TARGETS; i++) 
    {
        if (targets[i].active) active_count++;
    }

    if (active_count < MAX_TARGETS) 
    {
        for (int i = 0; i < MAX_TARGETS; i++) 
        {
            if (!targets[i].active) 
            {
                if ((rand() % 100) < TARGET_SPAWN_RATE) 
                {
                    // Spawn at least 5 units away from the walls
                    int margin = 5;
                    int tx = (rand() % (MAP_WIDTH - 2*margin)) + margin;
                    int ty = (rand() % (MAP_HEIGHT - 2*margin)) + margin;

                    // Drone distance
                    if (abs(tx - (int)drone->x) > 5 || abs(ty - (int)drone->y) > 5) 
                    {
                        targets[i].x = tx;
                        targets[i].y = ty;
                        targets[i].active = 1;
                        targets[i].value = 1;
                        return 1; // RETURN 1: We spawned something!
                    }
                }
            }
        }
    }
    return 0; // Nothing spawned
}

// COLLISION MANAGER 
int check_target_collision(Target targets[], const DroneState *drone) 
{
    int score_increment = 0;

    for (int i = 0; i < MAX_TARGETS; i++) 
    {
        if (targets[i].active) 
        {
            double dx = targets[i].x - drone->x;
            double dy = targets[i].y - drone->y;
            double distance = sqrt(dx*dx + dy*dy);

            if (distance < COLLECTION_RADIUS) 
            {
                targets[i].active = 0;
                score_increment += targets[i].value;
            }
        }
    }
    return score_increment;
}