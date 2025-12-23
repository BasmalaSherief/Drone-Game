#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

// GLOBAL FLAG FOR CLEANUP
volatile sig_atomic_t keep_running = 1;

void handle_signal(int sig) 
{
    keep_running = 0;
}

// Log function that appends to a file
void log_msg(const char *process_name, const char *format, ...)
 {
    FILE *f = fopen("simulation.log", "a");
    if (f == NULL) return; // Can't log if file sys is broken

    // Get current time
    time_t now;
    time(&now);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0'; 

    fprintf(f, "[%s] [%s] ", date, process_name);

    // Handle variable arguments like printf
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);
}