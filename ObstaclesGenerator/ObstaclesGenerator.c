#include <stdlib.h> 
#include <math.h>  
#include "../common.h" 
#include "ObstaclesGenerator.h"

// GENERATOR (Lifecycle Logic) 
void update_obstacle_lifecycle(Obstacle obstacles[], DroneState *drone) 
{   
    for (int i = 0; i < MAX_OBSTACLES; i++) 
    {  
        // Manage Active Obstacles
        if (obstacles[i].active) 
        {
            obstacles[i].timer--;
            if (obstacles[i].timer <= 0) {
                obstacles[i].active = 0; // Despawn
            }
        } 
        // Try to Spawn New Obstacles
        else 
        {
            // Random roll (0-99) < Chance
            if ((rand() % 100) < SPAWN_CHANCE) 
            {  
                // Generate random coords inside the map borders
                int cand_x = (rand() % (MAP_WIDTH - 2 * BORDER_MARGIN_SPAWN)) + BORDER_MARGIN_SPAWN;
                int cand_y = (rand() % (MAP_HEIGHT - 2 * BORDER_MARGIN_SPAWN)) + BORDER_MARGIN_SPAWN;

                // SAFETY CHECK: Calculate distance to drone
                double dx = cand_x - drone->x;
                double dy = cand_y - drone->y;
                double dist = sqrt(dx*dx + dy*dy);

                // Only spawn if far away from the drone 
                if (dist > SAFE_RADIUS) {
                    obstacles[i].x = cand_x;
                    obstacles[i].y = cand_y;
                    obstacles[i].active = 1;
                    obstacles[i].timer = OBSTACLE_LIFETIME;
                }
            }
        }
    }
}

// PHYSICS (Repulsive Logic) 
void apply_repulsive_forces(DroneState *drone, Obstacle obstacles[]) 
{  
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        // Ignore inactive obstacles
        if (!obstacles[i].active) continue;

        // Vector from Obstacle TO Drone (Pushing away)
        double dx = drone->x - obstacles[i].x;
        double dy = drone->y - obstacles[i].y;
        double distance = sqrt(dx*dx + dy*dy);

        // Check Range
        if (distance < INFLUENCE_RANGE && distance > 0.1) {
            
            /* Calculate Force Magnitude (Inverse Square Law)
             Formula: Gain * (1/dist - 1/range) * (1/dist^2)
             Simpler Game Version: Gain / dist^2 */
            
            double magnitude = REPULSIVE_GAIN / (distance * distance);

            // Cap the force to prevent "Teleporting" glitches
            if (magnitude > MAX_FORCE) magnitude = MAX_FORCE;

            // Calculate Unit Vector (Direction)
            double unit_x = dx / distance;
            double unit_y = dy / distance;

            // Apply to Drone
            drone->force_x += magnitude * unit_x;
            drone->force_y += magnitude * unit_y;
        }
    }
}

void apply_border_forces(DroneState *drone) 
{   
    // LEFT WALL (x = 0)
    if (drone->x < BORDER_MARGIN) 
    {
        double dist = drone->x;
        if (dist < 0.1) dist = 0.1; 
        double force = BORDER_GAIN / (dist * dist);
        if (force > MAX_FORCE) force = MAX_FORCE;
        drone->force_x += force; // Push RIGHT 
    }

    // RIGHT WALL (x = MAP_WIDTH)
    if (drone->x > MAP_WIDTH - BORDER_MARGIN) 
    {
        double dist = MAP_WIDTH - drone->x;
        if (dist < 0.1) dist = 0.1;
        double force = BORDER_GAIN / (dist * dist);
        if (force > MAX_FORCE) force = MAX_FORCE;
        drone->force_x -= force; // Push LEFT
    }

    // TOP WALL (y = 0)
    if (drone->y < BORDER_MARGIN) 
    {
        double dist = drone->y;
        if (dist < 0.1) dist = 0.1;
        double force = BORDER_GAIN / (dist * dist);
        if (force > MAX_FORCE) force = MAX_FORCE;
        drone->force_y += force; // Push DOWN 
    }

    // BOTTOM WALL (y = MAP_HEIGHT)
    if (drone->y > MAP_HEIGHT - BORDER_MARGIN) 
    {
        double dist = MAP_HEIGHT - drone->y;
        if (dist < 0.1) dist = 0.1;
        double force = BORDER_GAIN / (dist * dist);
        if (force > MAX_FORCE) force = MAX_FORCE;
        drone->force_y -= force; // Push UP 
    }
}
