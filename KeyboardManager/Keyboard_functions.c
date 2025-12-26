#include <ncurses.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include "../common.h"
#include "KeyboardManager.h"

// Draws the 3x3 control grid and highlights the active key
void draw_input_display(WINDOW *win, int last_ch) 
{
    box(win, 0, 0);
    mvwprintw(win, 0, 1, " INPUT CONTROLS ");
    
    // Instructional text
    mvwprintw(win, 2, 2, "Use ARROW KEYS to move");
    mvwprintw(win, 3, 2, "SPACE: Brake | S: Start");
    mvwprintw(win, 4, 2, "R: Reset     | Q: Quit");

    /*   [ ] [^] [ ]
         [<] [ ] [>]
         [ ] [v] [ ] */
    
    // Default attribute
    int attr_up = A_NORMAL;
    int attr_down = A_NORMAL;
    int attr_left = A_NORMAL;
    int attr_right = A_NORMAL;
    int attr_brake = A_NORMAL;
    int attr_q = A_NORMAL;

    // Highlight based on key press
    if (last_ch == KEY_UP)    attr_up = A_REVERSE;
    if (last_ch == KEY_DOWN)  attr_down = A_REVERSE;
    if (last_ch == KEY_LEFT)  attr_left = A_REVERSE;
    if (last_ch == KEY_RIGHT) attr_right = A_REVERSE;
    if (last_ch == ' ')       attr_brake = A_REVERSE;
    if (last_ch == 'q')       attr_q = A_REVERSE;

    // Draw the Virtual Keypad
    // UP
    wattron(win, attr_up);
    mvwprintw(win, 8, 12, "[ ^ ]");
    wattroff(win, attr_up);

    // LEFT
    wattron(win, attr_left);
    mvwprintw(win, 10, 6, "[ < ]");
    wattroff(win, attr_left);

    // BRAKE
    wattron(win, attr_brake);
    mvwprintw(win, 10, 12, "[BRK]");
    wattroff(win, attr_brake);

    // RIGHT
    wattron(win, attr_right);
    mvwprintw(win, 10, 18, "[ > ]");
    wattroff(win, attr_right);

    // DOWN
    wattron(win, attr_down);
    mvwprintw(win, 12, 12, "[ v ]");
    wattroff(win, attr_down);
    
    // QUIT BUTTON
    wattron(win, attr_q);
    mvwprintw(win, 16, 12, "[ Q ]");
    wattroff(win, attr_q);

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

    // Forces
    mvwprintw(win, 11, 2, "TOTAL FORCES (N):");
    mvwprintw(win, 12, 4, "Fx: %8.3f", state->drone.force_x);
    mvwprintw(win, 13, 4, "Fy: %8.3f", state->drone.force_y);

    // Game Status
    mvwprintw(win, 15, 2, "STATUS:");
    if (state->game_active) 
    {
        wattron(win, A_BOLD);
        mvwprintw(win, 16, 4, "[ ACTIVE ]");
        wattroff(win, A_BOLD);
    } 
    else 
    {
        mvwprintw(win, 16, 4, "[ PAUSED ]");
    }

    mvwprintw(win, 18, 2, "SCORE: %d", state->score);

    wrefresh(win);
}