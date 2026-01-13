#!/bin/bash

echo "--- DRONE GAME LAUNCHER ---"

# 1. CLEANUP
echo "[*] Killing old processes..."
pkill -f server
pkill -f drone
pkill -f keyboard
pkill -f obstacle_process
pkill -f target_process
pkill -f watchdog
# Clean pipes
rm -f /tmp/fifo*

# 2. BUILD
echo "[*] Building project..."
make -s
if [ $? -ne 0 ]; then
    echo "[!] Build failed. Exiting."
    exit 1
fi

# 3. SETUP CONFIGURATION (Prompts in Main Terminal)
echo "------------------------------------"
echo "Select Operation Mode:"
echo "1. Local (Standalone)"
echo "2. Networked - SERVER (Host)"
echo "3. Networked - CLIENT (Join)"
echo "------------------------------------"
read -p "Enter choice (1-3): " choice

# Write to param.conf
echo "# Auto-generated configuration" > param.conf
echo "PORT=5555" >> param.conf

if [ "$choice" == "2" ]; then
    echo "MODE=server" >> param.conf
    echo "[*] Configured as SERVER."
elif [ "$choice" == "3" ]; then
    echo "MODE=client" >> param.conf
    read -p "Enter Server IP Address: " ip_addr
    echo "SERVER_IP=$ip_addr" >> param.conf
    echo "[*] Configured as CLIENT connecting to $ip_addr."
else
    echo "MODE=standalone" >> param.conf
    echo "[*] Configured as STANDALONE."
fi

# 4. LAUNCH GAME
# We use 'konsole --hold' so the window stays open if it crashes/exits.
# This fixes the "terminates each time" visibility issue.
echo "[*] Launching Simulation Window..."
konsole --hold -e ./server &

echo "--- SESSION RUNNING IN NEW WINDOW ---"