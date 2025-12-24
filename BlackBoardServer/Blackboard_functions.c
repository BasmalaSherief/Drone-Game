#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ncurses.h> 
#include "Blackboard.h"
#include "../common.h"

/* ASSIGNMENT1 CORRECTION:
    - implemented the parent child communication through fork() 
    and exec() for executing the children processes
    - Added Error check after initializing ncurses window
*/
// Function to spawn a child process
pid_t spawn_process(const char *program, char *arg_list[]) 
{
    pid_t pid = fork();
    if (pid < 0) 
    {
        perror("Error forking process");
        return -1;
    }
    else if (pid == 0) 
    {
        // Child Process
        execvp(program, arg_list);
        // If execvp returns, it failed
        perror("Error executing process");
        exit(1);
    }
    // Parent returns the child PID
    return pid;
}

// Initialize NCURSES
void init_console() 
{
    initscr();
    if (initscr() == NULL) 
    {
        fprintf(stderr, "Error initializing ncurses for map display.\n");
        exit(1);
    }
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    init_pair(COLOR_DRONE, COLOR_BLUE, COLOR_BLACK);
    init_pair(COLOR_OBSTACLE, COLOR_YELLOW, COLOR_BLACK); 
    init_pair(COLOR_TARGET, COLOR_GREEN, COLOR_BLACK);
}

void draw_map(WorldState *world) 
{
    erase(); 
    
    // Draw Borders dynamically based on current window size
    box(stdscr, 0, 0);
    mvprintw(0, 2, " MAP DISPLAY ");
    mvprintw(0, 20, " Score: %d ", world->score);
    mvprintw(0, 40, " Size: %dx%d ", COLS, LINES); 

    // Scaling Factors
    // Map internal coordinates (0-80) to Screen coordinates (0-COLS)
    float scale_x = (float)COLS / MAP_WIDTH;
    float scale_y = (float)LINES / MAP_HEIGHT;

    // Draw Obstacles 
    attron(COLOR_PAIR(COLOR_OBSTACLE));
    for(int i=0; i<MAX_OBSTACLES; i++) {
        if(world->obstacles[i].active) {
            int screen_x = (int)(world->obstacles[i].x * scale_x);
            int screen_y = (int)(world->obstacles[i].y * scale_y);
            // Bounds check to keep inside box
            if(screen_x >= COLS-1) screen_x = COLS-2;
            if(screen_y >= LINES-1) screen_y = LINES-2;
            if(screen_x < 1) screen_x = 1;
            if(screen_y < 1) screen_y = 1;
            
            mvaddch(screen_y, screen_x, 'O');
        }
    }
    attroff(COLOR_PAIR(COLOR_OBSTACLE));

    // Draw Targets 
    attron(COLOR_PAIR(COLOR_TARGET));
    for(int i=0; i<MAX_TARGETS; i++) {
        if(world->targets[i].active) {
            int screen_x = (int)(world->targets[i].x * scale_x);
            int screen_y = (int)(world->targets[i].y * scale_y);
            
            if(screen_x >= COLS-1) screen_x = COLS-2;
            if(screen_y >= LINES-1) screen_y = LINES-2;
            if(screen_x < 1) screen_x = 1;
            if(screen_y < 1) screen_y = 1;

            mvaddch(screen_y, screen_x, 'T');
        }
    }
    attroff(COLOR_PAIR(COLOR_TARGET));

    // Draw Drone 
    attron(COLOR_PAIR(COLOR_DRONE));
    int drone_screen_x = (int)(world->drone.x * scale_x);
    int drone_screen_y = (int)(world->drone.y * scale_y);
    
    if(drone_screen_x >= COLS-1) drone_screen_x = COLS-2;
    if(drone_screen_y >= LINES-1) drone_screen_y = LINES-2;
    if(drone_screen_x < 1) drone_screen_x = 1;
    if(drone_screen_y < 1) drone_screen_y = 1;

    mvaddch(drone_screen_y, drone_screen_x, '+');
    attroff(COLOR_PAIR(COLOR_DRONE));

    refresh();
}