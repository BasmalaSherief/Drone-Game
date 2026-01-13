#!/bin/bash

echo "--- DRONE GAME LAUNCHER ---"

# CLEANUP
echo "[*] Killing old processes..."
pkill -f server
pkill -f drone
pkill -f keyboard
pkill -f obstacle_process
pkill -f target_process
pkill -f watchdog

# BUILD
echo "[*] Cleaning previous build..."
make clean
echo "[*] Building project..."
make
if [ $? -ne 0 ]; then
    echo "[!] Build failed. Exiting."
    exit 1
fi

# CLEANUP PIPES
echo "[*] Cleaning up old pipes..."
rm -f /tmp/fifo*

# CLEAR LOG
echo "[*] Clearing log file..."
> simulation.log 

# LAUNCH BLACKBOARD SERVER
# Fix: Run directly in this terminal, not in a new konsole window.
echo "[*] Launching Blackboard Server..."
./server

echo "--- GAME SESSION ENDED ---"