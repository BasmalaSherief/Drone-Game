#ifndef KEYBOARDMANAGER_H
#define KEYBOARDMANAGER_H

#include <ncurses.h>
#include "../common.h"

//FUNCTIONS

// Draws the 3x3 control grid and highlights the active key
void draw_input_display(WINDOW *win, int last_ch) ;

// Displays the Physics Data received from Blackboard
void draw_dynamics_display(WINDOW *win, WorldState *state);

#endif 