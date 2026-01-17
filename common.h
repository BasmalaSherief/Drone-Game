#ifndef COMMON_H
#define COMMON_H

// MAP SETTINGS 
#define MAP_WIDTH 80
#define MAP_HEIGHT 24

// LIMITS 
#define MAX_OBSTACLES 10
#define MAX_TARGETS 10
#define TIMEOUT_SECONDS 4 // If no heartbeat for 4 seconds, kill system

// GAME CONFIGURATION
#define TOTAL_TARGETS_TO_WIN 10

// NETWORK SETTINGS
#define BUFFER_CAP 1024       
#define SYNC_RATE_US 30000 
#define RETRY_SEC 1

// KEYBOARD INPUT (Input Process -> Drone Process) 
typedef struct {
    float force_x;    
    float force_y;    
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

// Structure to send (Targets + Score gained this frame) to Server 
typedef struct {
    Target targets[MAX_TARGETS];
    int score_increment;
} TargetPacket;

// THE WORLD STATE (Master Process -> Display Process) 
typedef struct {
    DroneState drone;
    Obstacle obstacles[MAX_OBSTACLES];
    Target targets[MAX_TARGETS];
    int score;
    int game_active; // 0=Paused, 1=Flying
} WorldState;

// NETWORK COMMUNICATION STRUCTURES

// Operation Mode
typedef enum {
    MODE_STANDALONE,  // Normal Assignment 2 operation
    MODE_SERVER,      // Server: Share drone, receive obstacle
    MODE_CLIENT       // Client: Send drone, display remote
} OperationMode;

// Network Message Types (for protocol)
typedef enum {
    MSG_OK,           // Handshake: "ok"
    MSG_OOK,          // Acknowledgment: "ook"
    MSG_SIZE,         // Window size: "size"
    MSG_SOK,          // Size acknowledgment: "sok"
    MSG_DRONE,        // Drone position: "drone"
    MSG_DOK,          // Drone acknowledgment: "dok"
    MSG_OBST,         // Obstacle position: "obst"
    MSG_POK,          // Obstacle acknowledgment: "pok"
    MSG_QUIT,         // Quit command: "q"
    MSG_QOK           // Quit acknowledgment: "qok"
} MessageType;

// Network Packet Structure
typedef struct {
    int conn_fd;
    int pipe_in_fd;   // Reads Local Drone (fifoBBObs)
    int pipe_out_fd;  // Writes Remote Obstacle (fifoObsBB)
    int role; 
    
    // Internal Buffer State
    char net_buffer[BUFFER_CAP];
    int buf_start;
    int buf_end;
} LinkContext;

// Log function that appends to a file
void log_msg(const char *process_name, const char *format, ...);

#endif