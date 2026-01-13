#ifndef BLACKBOARD_H
#define BLACKBOARD_H

#include <sys/types.h>
#include "../common.h"

// Ncurses Colors
#define COLOR_DRONE     1
#define COLOR_OBSTACLE  2
#define COLOR_TARGET    3

// FUNCTIONS

// Handles the startup menu
void prompt_for_mode();

// Function to spawn a child process
pid_t spawn_process(const char *program, char *arg_list[]);

// Initialize NCURSES
void init_console();

void draw_map(WorldState *world);

#endif 
