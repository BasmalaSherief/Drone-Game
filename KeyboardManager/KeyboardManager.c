#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include <string.h> 
#include <errno.h>
#include <signal.h>
#include "../common.h"


// GLOBAL FLAG FOR CLEANUP
volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) 
{
    keep_running = 0;
}

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

int main() 
{
    // REGISTER SIGNALS
    signal(SIGINT, handle_signal);  // Ctrl+C
    signal(SIGTERM, handle_signal); // Kill command

    // PIPE SETUP + Checking their errros
    const char *fifoKD = "/tmp/fifoKD";       // Write commands to Drone
    const char *fifoBBDIS = "/tmp/fifoBBDIS"; // Read state from Blackboard
    
    if (mkfifo(fifoKD, 0666) == -1 && errno != EEXIST) { perror("Keyboard: Failed to create fifoKD"); exit(EXIT_FAILURE); }

    if (mkfifo(fifoBBDIS, 0666) == -1 && errno != EEXIST) { perror("Keyboard: Failed to create fifoBBDIS"); exit(EXIT_FAILURE); }

    int fd_KD = open(fifoKD, O_WRONLY);  
    if (fd_KD == -1) { perror("Pipe From Keyboard to Drone: open write"); exit(1); }
    // Non-blocking read for display data so input doesn't lag
    int fd_BBDIS = open(fifoBBDIS, O_RDONLY | O_NONBLOCK);
    if (fd_BBDIS == -1) { perror("Pipe From Blackboard server to Keyboard Manager: open read"); exit(1); }

    // NCURSES SETUP
    initscr();
    if (initscr() == NULL) 
    {
    fprintf(stderr, "Error initializing ncurses for input display.\n");
    exit(1);
    }
    cbreak();               
    noecho();               
    keypad(stdscr, TRUE);   // ENABLE ARROW KEYS 
    nodelay(stdscr, TRUE);  // Make getch() non-blocking
    curs_set(0);            

    // DYNAMIC WINDOW SETUP 
    int input_win_width = COLS / 2;  
    int dyn_win_width = COLS - input_win_width; 

    WINDOW *win_input = newwin(LINES, input_win_width, 0, 0); 
    WINDOW *win_dynamics = newwin(LINES, dyn_win_width, 0, input_win_width);

    // Data containers
    InputMsg msg;
    WorldState current_state; 
    
    // Init state to zero
    memset(&current_state, 0, sizeof(WorldState));

    // MAIN LOOP
    while(keep_running) 
    {
        int ch;
        int last_ch = 0;
        int current_fx = 0, current_fy = 0;
        char cmd = 0;
        int key_pressed = 0;

        // CHECK FOR RESIZE
        // If the user resized the terminal, we need to resize the internal windows
        if (KEY_RESIZE) 
        {
            resize_term(0, 0); // Update ncurses internal structures
            
            // Recalculate widths
            input_win_width = COLS / 2;
            dyn_win_width = COLS - input_win_width;

            // Resize and Move windows
            wresize(win_input, LINES, input_win_width);
            mvwin(win_input, 0, 0);
            
            wresize(win_dynamics, LINES, dyn_win_width);
            mvwin(win_dynamics, 0, input_win_width);
            
            // Clear to remove artifacts
            erase();
            refresh();
        }

        // INPUT HANDLING (Buffer Flush)
        while((ch = getch()) != ERR) 
        {
            key_pressed = 1;
            if (ch == KEY_RESIZE) continue; // Ignore resize events 
            last_ch = ch;

            // Map ARROWS to Forces 
            switch(ch) 
            {
                case KEY_UP:    current_fy = -1; break;
                case KEY_DOWN:  current_fy =  1; break;
                case KEY_LEFT:  current_fx = -1; break;
                case KEY_RIGHT: current_fx =  1; break;
                
                // Map COMMANDS 
                case 'q': cmd = 'q'; break; 
                case 's': cmd = 's'; break;
                case 'r': cmd = 'r'; break;
                case ' ': cmd = ' '; break; 
            }
            //  Only log if the key is different from the last frame's key
            // OR if it's a command key (s, r, q) which is always important.
            static int prev_logged_ch = -1;
            if (ch != prev_logged_ch || ch == 's' || ch == 'r' || ch == 'q' || ch == ' ') 
            {
                if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT || 
                        ch == 's' || ch == 'r' || ch == 'q' || ch == ' ') 
                        {
                            log_msg("KEYBOARD", "User pressed key code: %d", ch);
                            prev_logged_ch = ch;
                        }
            }
        }

        // SEND COMMAND TO DRONE 
        msg.force_x = current_fx;
        msg.force_y = current_fy;
        msg.command = cmd;

        ssize_t bytesWrittenKD = write(fd_KD, &msg, sizeof(msg));

        if (bytesWrittenKD == -1) 
        {
            if (errno != EPIPE && errno != EAGAIN) 
            {
                log_msg("KEYBOARD", "Critical: Failed to write command to Drone. Error: %s", strerror(errno));
            }
            else if (errno == EPIPE)
            {
                // Log warning that drone is gone
                log_msg("KEYBOARD", "Warning: Drone process disconnected (Broken Pipe).");
            }
        }

        // If Quit was pressed, exit the loop immediately
        if(cmd == 'q') 
        {
            log_msg("KEYBOARD", "Sent QUIT command. Exiting loop.");
            keep_running = 0;
        }

        else if (cmd != 0) log_msg("INPUT", "Command received: %c", cmd);

        // READ DATA FROM BLACKBOARD
        // Non-blocking read
        ssize_t bytes = read(fd_BBDIS, &current_state, sizeof(WorldState));
        if (bytes == -1) 
        {
            if (errno != EAGAIN) 
            {
                log_msg("KEYBOARD", "Error reading from Blackboard Pipe: %s", strerror(errno));
            }
        } 
        else if (bytes == 0) 
        {
            fprintf(stderr, "Keyboard: Server closed connection\n");
            keep_running = 0;
        }

        // UPDATE DISPLAYS 
        draw_input_display(win_input, last_ch);
        
        // Only update dynamics if we actually have data (or at least draw the initial frame)
        if (bytes > 0 || current_state.drone.x != 0) 
        {
            draw_dynamics_display(win_dynamics, &current_state);
        }

        // TIMING
        // 30ms sleep 
        usleep(30000);
    }

    // CLEANUP
    delwin(win_input);
    delwin(win_dynamics);
    endwin();
    close(fd_KD);
    close(fd_BBDIS);
    log_msg("KEYBOARD", "Exiting cleanly");
    return 0;
}