#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <ncurses.h> 
#include "Blackboard.h"
#include "../common.h"

/* ASSIGNMENT1 CORRECTION:
    - implemented the parent child communication through fork() 
    and exec() for executing the children processes
    - Added Error check after initializing ncurses window
*/

// Handles the startup menu
void prompt_for_mode() 
{
    int choice;
    char ip[32];
    char line[256];
    
    printf("\n====================================\n");
    printf("   DRONE GAME - STARTUP CONFIG    \n");
    printf("====================================\n");
    printf("Select Operation Mode:\n");
    printf("1. Local (Standalone) \n");
    printf("2. Networked - SERVER (Host)\n");
    printf("3. Networked - CLIENT (Join)\n");
    printf("------------------------------------\n");
    printf("Enter choice (1-3): ");
    
    if (scanf("%d", &choice) != 1) choice = 1; // Default to local on error
    
    // Flush stdin to remove newline characters
    while(getchar() != '\n'); 

    // If Client, ask for IP
    if (choice == 3) 
    {
        printf("Enter Server IP Address: ");
        char input[100];
        if (fgets(input, sizeof(input), stdin)) 
        {
            // Remove newline
            input[strcspn(input, "\n")] = 0;
            if (strlen(input) > 0) 
            {
                strncpy(ip, input, 31);
            }
        }
    }

    // Write to param.conf
    FILE *f = fopen("param.conf", "w");
    if (!f) 
    {
        perror("Failed to write param.conf");
        sleep(2);
        return;
    }

    fprintf(f, "# Auto-generated configuration\n");
    fprintf(f, "PORT=5555\n"); 

    switch(choice) 
    {
        case 2:
            fprintf(f, "MODE=server\n");
            printf(">> Mode set to SERVER. Waiting for connection...\n");
            break;
        case 3:
            fprintf(f, "MODE=client\n");
            fprintf(f, "SERVER_IP=%s\n", ip);
            printf(">> Mode set to CLIENT. Target: %s\n", ip);
            break;
        default:
            fprintf(f, "MODE=standalone\n");
            printf(">> Mode set to STANDALONE.\n");
            break;
    }
    
    fclose(f);
    sleep(1); // Short pause so user can read the status
}

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
    for(int i=0; i<MAX_TARGETS; i++) 
    {
        if(world->targets[i].active) 
        {
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