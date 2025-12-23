#ifndef COMMON_H
#define COMMON_H

// MAP SETTINGS 
#define MAP_WIDTH 80
#define MAP_HEIGHT 24

// LIMITS 
#define MAX_OBSTACLES 10
#define MAX_TARGETS 10

// KEYBOARD INPUT (Input Process -> Drone Process) 
typedef struct {
    int force_x;    // -1, 0, 1
    int force_y;    // -1, 0, 1
    char command;   // 's', 'r', 'q', ' '
} InputMsg;

// SUB-COMPONENTS 
typedef struct {
    double x, y;
    double vx, vy;
    double force_x, force_y;
} DroneState;

typedef struct {
    int x, y;       // Coordinates for the grid
    int active;     // 0 or 1
    int timer;      // For obstacles
} Obstacle;

typedef struct {
    int x, y;
    int active;     // 0 or 1
    int value;      // For scoring
} Target;

// THE WORLD STATE (Master Process -> Display Process) 
typedef struct {
    DroneState drone;
    Obstacle obstacles[MAX_OBSTACLES];
    Target targets[MAX_TARGETS];
    int score;
    int game_active; // 0=Paused, 1=Flying
} WorldState;


// Log function that appends to a file
void log_msg(const char *process_name, const char *format, ...);

#endif