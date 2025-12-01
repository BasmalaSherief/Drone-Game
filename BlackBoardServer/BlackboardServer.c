#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <ncurses.h> 
#include <string.h>

#include "../common.h"
#include "../TargetGenerator/TargetGenerator.c"
#include "../ObstaclesGenerator/ObstaclesGenerator.c"

// GAME CONFIGURATION
#define TOTAL_TARGETS_TO_WIN 10

// Ncurses Colors
#define COLOR_DRONE     1
#define COLOR_OBSTACLE  2
#define COLOR_TARGET    3

void init_console() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    init_pair(COLOR_DRONE, COLOR_BLUE, COLOR_BLACK);
    init_pair(COLOR_OBSTACLE, COLOR_YELLOW, COLOR_BLACK); 
    init_pair(COLOR_TARGET, COLOR_GREEN, COLOR_BLACK);
}

void draw_map(WorldState *world) {
    erase(); 
    
    // Draw Borders
    box(stdscr, 0, 0);
    mvprintw(0, 2, " MAP DISPLAY ");
    mvprintw(0, 20, " Score: %d ", world->score);

    // Draw Obstacles 
    attron(COLOR_PAIR(COLOR_OBSTACLE));
    for(int i=0; i<MAX_OBSTACLES; i++) {
        if(world->obstacles[i].active) {
            mvaddch((int)world->obstacles[i].y, (int)world->obstacles[i].x, 'O');
        }
    }
    attroff(COLOR_PAIR(COLOR_OBSTACLE));

    // Draw Targets 
    attron(COLOR_PAIR(COLOR_TARGET));
    for(int i=0; i<MAX_TARGETS; i++) {
        if(world->targets[i].active) {
            mvaddch((int)world->targets[i].y, (int)world->targets[i].x, 'T');
        }
    }
    attroff(COLOR_PAIR(COLOR_TARGET));

    // Draw Drone 
    attron(COLOR_PAIR(COLOR_DRONE));
    mvaddch((int)world->drone.y, (int)world->drone.x, '+');
    attroff(COLOR_PAIR(COLOR_DRONE));

    refresh();
}

int main() 
{
    srand(time(NULL));

    // DATA INIT 
    WorldState world;
    world.drone.x = MAP_WIDTH / 2.0;
    world.drone.y = MAP_HEIGHT / 2.0;
    world.drone.vx = 0; world.drone.vy = 0;
    world.score = 0;
    world.game_active = 0;

    for(int i=0; i<MAX_OBSTACLES; i++) world.obstacles[i].active = 0;
    for(int i=0; i<MAX_TARGETS; i++) world.targets[i].active = 0;

    // GAME COUNTERS
    int targets_spawned_total = 0;
    int targets_collected_total = 0;

    // PIPES 
    const char *fifoDBB = "/tmp/fifoDBB";   
    const char *fifoBBD = "/tmp/fifoBBD";   
    const char *fifoBBDIS = "/tmp/fifoBBDIS"; 

    mkfifo(fifoDBB, 0666);
    mkfifo(fifoBBD, 0666);
    mkfifo(fifoBBDIS, 0666);

    // NCURSES INIT
    init_console();

    // Non-blocking open for pipes
    int fd_DBB = open(fifoDBB, O_RDONLY | O_NONBLOCK);
    int fd_BBD = open(fifoBBD, O_WRONLY); 
    int fd_BBDIS = open(fifoBBDIS, O_WRONLY);

    DroneState incoming_drone_state;

    while(1) {
        // READ INPUT 
        ssize_t bytesRead = read(fd_DBB, &incoming_drone_state, sizeof(DroneState));

        if (bytesRead > 0) 
        {
            if (incoming_drone_state.x == -1.0) break;

            if (incoming_drone_state.x == -2.0) 
            {
                // Reset World State
                world.score = 0;
                world.game_active = 0;
                
                // Reset Counters
                targets_spawned_total = 0;
                targets_collected_total = 0;

                // Clear Arrays (Set active = 0)
                for(int i=0; i<MAX_OBSTACLES; i++) world.obstacles[i].active = 0;
                for(int i=0; i<MAX_TARGETS; i++) world.targets[i].active = 0;
                
                // Reset Drone Position visually
                world.drone.x = 10.0;
                world.drone.y = 10.0;

                continue; 
            }

            world.drone = incoming_drone_state;
            world.game_active = 1; 
        }

        // LOGIC 
        if (world.game_active) 
        {
            // Obstacles 
            update_obstacle_lifecycle(world.obstacles, &world.drone);

            // Targets (Spawn Logic)
            if (targets_spawned_total < TOTAL_TARGETS_TO_WIN) 
            {
                int spawned = refresh_targets(world.targets, &world.drone);
                targets_spawned_total += spawned;
            }

            // Targets (Collision Logic)
            int points = check_target_collision(world.targets, &world.drone);
            if(points > 0) 
            {
                world.score += points;
                targets_collected_total += points;
            }

            // CHECK WIN CONDITION
            if (targets_collected_total >= TOTAL_TARGETS_TO_WIN) 
            {
                erase();
                attron(A_BOLD | COLOR_PAIR(COLOR_TARGET));
                mvprintw(MAP_HEIGHT/2, MAP_WIDTH/2 - 5, "YOU WIN!");
                mvprintw(MAP_HEIGHT/2 + 1, MAP_WIDTH/2 - 10, "Final Score: %d", world.score);
                attroff(A_BOLD | COLOR_PAIR(COLOR_TARGET));
                refresh();
                sleep(4); 
                break;   
            }
        }

        // DISPLAY
        draw_map(&world);

        // BROADCAST 
        write(fd_BBD, world.obstacles, sizeof(world.obstacles));
        write(fd_BBDIS, &world, sizeof(WorldState));

        usleep(30000); 
    }

    // Cleanup
    endwin();
    // Force kill other processes
    system("pkill -f drone"); 
    system("pkill -f keyboard");
    
    close(fd_DBB);
    close(fd_BBD);
    close(fd_BBDIS);
    return 0;
}