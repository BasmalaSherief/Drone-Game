#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include "../common.h"


// Global last beat time
volatile sig_atomic_t keep_running = 1;

// HANDLER: Triggered when the Alarm goes off 
void handle_alarm(int sig) 
{
    log_msg("WATCHDOG", "ALERT: No heartbeat received for %d seconds. Killing system.", TIMEOUT_SECONDS);   
    // Kill the Parent Process (Server)
    kill(getppid(), SIGTERM);
    exit(EXIT_FAILURE);
}

// HANDLER: Triggered when Server is alive
void handle_heartbeat(int sig) 
{
    // Reset the alarm clock
    alarm(TIMEOUT_SECONDS);
}

// HANDLER: Clean exit
void handle_term(int sig)
{
    keep_running = 0;
}

int main(int argc, char *argv[]) 
{
    // Register Signals
    signal(SIGALRM, handle_alarm);     // The "Time's up" signal
    signal(SIGUSR1, handle_heartbeat); // The "Heartbeat" signal
    signal(SIGTERM, handle_term);      // The "Quit" signal

    log_msg("WATCHDOG", "Started. Waiting for signals...");

    // Start the first countdown
    alarm(TIMEOUT_SECONDS);

    // Wait forever 
    while (keep_running) 
    {
        pause(); // Suspends process until a signal arrives
    }

    log_msg("WATCHDOG", "Terminating.");
    return 0;
}