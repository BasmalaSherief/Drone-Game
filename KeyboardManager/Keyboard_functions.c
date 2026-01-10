#include <ncurses.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include "../common.h"
#include "KeyboardManager.h"

// Draws the 3x3 control grid and highlights the active key
void draw_input_display(WINDOW *win, int last_ch) 
{
    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 0, 1, " INPUT CONTROLS ");
    
    // Instructional text
    mvwprintw(win, 2, 2, "ARROW KEYS = Move (8 directions)");
    mvwprintw(win, 3, 2, "Combine for diagonals:");
    wattron(win, A_BOLD);
    mvwprintw(win, 4, 3, "UP+LEFT=NW  UP+RIGHT=NE");
    wattroff(win, A_BOLD);
    mvwprintw(win, 6, 2, "SPACE = Brake  | S = Start");
    mvwprintw(win, 7, 2, "R = Reset      | Q = Quit");

    /*  VISUAL LAYOUT:
     *    [NW] [^]  [NE]
     *    [<]  [BRK] [>]
     *    [SW] [v]  [SE]
     */
    
    // Default attribute
    int attr_up = A_NORMAL;
    int attr_down = A_NORMAL;
    int attr_left = A_NORMAL;
    int attr_right = A_NORMAL;
    int attr_nw = A_NORMAL;  // Northwest (UP + LEFT)
    int attr_ne = A_NORMAL;  // Northeast (UP + RIGHT)
    int attr_sw = A_NORMAL;  // Southwest (DOWN + LEFT)
    int attr_se = A_NORMAL;  // Southeast (DOWN + RIGHT)
    int attr_brake = A_NORMAL;
    int attr_q = A_NORMAL;

    // Highlight based on key press - THIS IS THE FIX!
    if (last_ch == KEY_UP)    attr_up = A_REVERSE | A_BOLD;
    if (last_ch == KEY_DOWN)  attr_down = A_REVERSE | A_BOLD;
    if (last_ch == KEY_LEFT)  attr_left = A_REVERSE | A_BOLD;
    if (last_ch == KEY_RIGHT) attr_right = A_REVERSE | A_BOLD;
    if (last_ch == ' ')       attr_brake = A_REVERSE | A_BOLD;
    if (last_ch == 'q')       attr_q = A_REVERSE | A_BOLD;

    // DRAW THE 3x3 DIRECTIONAL KEYPAD
    int grid_top = 10;
    int grid_left = 6;
    
    // Top Row: [NW] [UP] [NE]
    wattron(win, attr_nw);
    mvwprintw(win, grid_top, grid_left, "[NW]");
    wattroff(win, attr_nw);
    
    wattron(win, attr_up);
    mvwprintw(win, grid_top, grid_left + 6, "[ ^ ]");
    wattroff(win, attr_up);
    
    wattron(win, attr_ne);
    mvwprintw(win, grid_top, grid_left + 13, "[NE]");
    wattroff(win, attr_ne);
    
    // Middle Row: [LEFT] [BRAKE] [RIGHT]
    wattron(win, attr_left);
    mvwprintw(win, grid_top + 2, grid_left, "[ < ]");
    wattroff(win, attr_left);
    
    wattron(win, attr_brake);
    mvwprintw(win, grid_top + 2, grid_left + 6, "[BRK]");
    wattroff(win, attr_brake);
    
    wattron(win, attr_right);
    mvwprintw(win, grid_top + 2, grid_left + 12, "[ > ]");
    wattroff(win, attr_right);
    
    // Bottom Row: [SW] [DOWN] [SE]
    wattron(win, attr_sw);
    mvwprintw(win, grid_top + 4, grid_left, "[SW]");
    wattroff(win, attr_sw);
    
    wattron(win, attr_down);
    mvwprintw(win, grid_top + 4, grid_left + 6, "[ v ]");
    wattroff(win, attr_down);
    
    wattron(win, attr_se);
    mvwprintw(win, grid_top + 4, grid_left + 13, "[SE]");
    wattroff(win, attr_se);
    
    // DIRECTION INDICATOR (Shows current movement vector)
    mvwprintw(win, grid_top + 7, 2, "ACTIVE DIRECTION:");
    
    const char *dir_name = "[ IDLE ]";
    
    // DIAGONAL HINT
    wattron(win, A_DIM);
    mvwprintw(win, grid_top + 10, 2, "Press 2 arrows at once");
    mvwprintw(win, grid_top + 11, 2, "     for diagonal movement");
    wattroff(win, A_DIM);
    
    // QUIT BUTTON
    wattron(win, attr_q);
    mvwprintw(win, grid_top + 14, grid_left + 6, "[ Q ]");
    wattroff(win, attr_q);
    wattron(win, A_DIM);
    mvwprintw(win, grid_top + 15, 4, "(Press Q to Quit)");
    wattroff(win, A_DIM);
    
    wrefresh(win);
}

// Displays the Physics Data received from Blackboard
void draw_dynamics_display(WINDOW *win, WorldState *state) 
{
    box(win, 0, 0);
    mvwprintw(win, 0, 1, " TELEMETRY ");

    // Position
    mvwprintw(win, 3, 2, "POSITION (m):");
    mvwprintw(win, 4, 4, "X: %8.3f", state->drone.x);
    mvwprintw(win, 5, 4, "Y: %8.3f", state->drone.y);

    // Velocity
    mvwprintw(win, 7, 2, "VELOCITY (m/s):");
    mvwprintw(win, 8, 4, "Vx: %8.3f", state->drone.vx);
    mvwprintw(win, 9, 4, "Vy: %8.3f", state->drone.vy);

    // Speed (magnitude)
    double speed = sqrt(state->drone.vx * state->drone.vx + 
                       state->drone.vy * state->drone.vy);
    mvwprintw(win, 10, 4, "Speed: %6.3f", speed);
    
    // Forces
    mvwprintw(win, 12, 2, "TOTAL FORCES (N):");
    mvwprintw(win, 13, 4, "Fx: %8.3f", state->drone.force_x);
    mvwprintw(win, 14, 4, "Fy: %8.3f", state->drone.force_y);
    
    // Force magnitude
    double force_mag = sqrt(state->drone.force_x * state->drone.force_x + 
                           state->drone.force_y * state->drone.force_y);
    mvwprintw(win, 15, 4, "Total: %6.3f", force_mag);

    // Game Status
    mvwprintw(win, 17, 2, "STATUS:");
    if (state->game_active) 
    {
        wattron(win, A_BOLD | COLOR_PAIR(2)); // Green 
        mvwprintw(win, 18, 4, "[ ACTIVE ]");
        wattroff(win, A_BOLD | COLOR_PAIR(2));
    } 
    else 
    {
        wattron(win, COLOR_PAIR(1)); // Red/Yellow 
        mvwprintw(win, 18, 4, "[ PAUSED ]");
        wattroff(win, COLOR_PAIR(1));
    }
    
    // Score
    mvwprintw(win, 20, 2, "SCORE: %d", state->score);
    
    // Movement mode indicator
    mvwprintw(win, 22, 2, "MODE: 8-Direction");
    
    wrefresh(win);
}