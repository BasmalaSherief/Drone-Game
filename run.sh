#!/bin/bash

echo "--- DRONE GAME LAUNCHER ---"

# 1. CLEANUP
echo "[*] Cleaning up old pipes..."
rm -f /tmp/fifo*

# 2. BLACKBOARD
# Point to the folder: BlackBoardServer/server
echo "[*] Launching Blackboard Server..."
konsole -e ./server & 
sleep 1

# 3. LOGIC PROCESSES
# Point to their specific folders

echo "[*] Launching Drone Dynamics..."
./drone &

sleep 0.5

# 4. KEYBOARD MANAGER
# Point to the folder: KeyboardManager/keyboard
echo "[*] Launching Keyboard Manager..."
konsole -e ./keyboard &

echo "--- SYSTEM RUNNING ---"