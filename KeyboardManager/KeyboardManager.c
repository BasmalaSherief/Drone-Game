#include <stdlib.h>
#include <math.h>
#include <ncurses.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include <string.h> 
#include <errno.h>
#include <signal.h>
#include "../common.h"
#include "KeyboardManager.h"

// GLOBAL FLAG FOR CLEANUP
volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) 
{
    keep_running = 0;
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
        char cmd = 0;
        float fx = 0.0, fy = 0.0;  // Force components

        // RESET key tracking each frame 
        bool keys_pressed[512] = {false}; // Local array, reset each frame

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
            if (ch == KEY_RESIZE) continue; // Ignore resize events 
            last_ch = ch;

            keys_pressed[ch] = true;  // Mark this key as pressed
            last_ch = ch;  // Track for display purposes

            // Map ARROWS to Forces 
            switch(ch) 
            {              
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

        // CALCULATE FORCE VECTOR (Supporting 8 directions)
        bool up    = keys_pressed[KEY_UP];
        bool down  = keys_pressed[KEY_DOWN];
        bool left  = keys_pressed[KEY_LEFT];
        bool right = keys_pressed[KEY_RIGHT];
        
        // Diagonal movements (8 directions total)
        if (up && right) 
        {
            // Northeast diagonal
            fx = 1.0 / sqrt(2.0);   // ~0.707
            fy = -1.0 / sqrt(2.0);  // Negative Y = up on screen
        }
        else if (up && left) 
        {
            // Northwest diagonal
            fx = -1.0 / sqrt(2.0);
            fy = -1.0 / sqrt(2.0);
        }
        else if (down && right) 
        {
            // Southeast diagonal
            fx = 1.0 / sqrt(2.0);
            fy = 1.0 / sqrt(2.0);
        }
        else if (down && left) 
        {
            // Southwest diagonal
            fx = -1.0 / sqrt(2.0);
            fy = 1.0 / sqrt(2.0);
        }
        // Cardinal directions (if no diagonal detected)
        else if (up) 
        {
            fx = 0.0;
            fy = -1.0;  // Up
        }
        else if (down) 
        {
            fx = 0.0;
            fy = 1.0;   // Down
        }
        else if (left) 
        {
            fx = -1.0;  // Left
            fy = 0.0;
        }
        else if (right) 
        {
            fx = 1.0;   // Right
            fy = 0.0;
        }

        // SEND COMMAND TO DRONE 
        msg.force_x = fx;  // Normalized direction (-1 to +1)
        msg.force_y = fy;
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