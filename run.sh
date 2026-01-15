#!/bin/bash

echo "========================================"
echo "      DRONE GAME LAUNCHER               "
echo "========================================"

# CLEANUP OLD PROCESSES
echo "[*] Cleaning up old processes..."
pkill -f server
pkill -f drone
pkill -f keyboard
pkill -f obstacle_process
pkill -f target_process
pkill -f watchdog
pkill -f network_process
rm -f /tmp/fifo*

# COMPILE 
echo "[*] Compiling..."
make -s
if [ $? -ne 0 ]; then
    echo "[!] Compilation Failed! Check your code."
    exit 1
fi

# DETECT LOCAL IP 
# Tries to find the main IP address (hostname -I) to show the user
# We capture the first IP found to use as the default for the Server
MY_IP=$(hostname -I | awk '{print $1}')
echo "----------------------------------------"
echo " Your Local IP Address is: $MY_IP"
echo " (Share this if you are the Server)"
echo "----------------------------------------"

# SELECT MODE
echo "Select Operation Mode:"
echo "  1. Local (Standalone)"
echo "  2. Networked - SERVER (Host)"
echo "  3. Networked - CLIENT (Join)"
read -p "Enter choice [1-3]: " choice

# GENERATE param.conf
# We overwrite the file every time based on user input.

echo "# Auto-generated config by run.sh" > param.conf

if [ "$choice" == "2" ]; then
    # --- SERVER MODE ---
    echo "MODE=server" >> param.conf
    
    # Use the detected machine 
    echo "SERVER_IP=$MY_IP" >> param.conf
    
    # Use the default port (5555) automatically
    echo "PORT=5555" >> param.conf
    
    echo "[*] Starting as SERVER on $MY_IP:5555..."

elif [ "$choice" == "3" ]; then
    # --- CLIENT MODE ---
    echo "MODE=client" >> param.conf
    
    # Ask for the Server's IP
    read -p "Enter Server IP Address (e.g., 192.168.1.X): " user_ip
    
    # Ask for the Server's Port
    read -p "Enter Server Port (default 5555): " user_port
    
    # Validation: If user presses enter without typing, set defaults
    if [ -z "$user_ip" ]; then
        user_ip=$MY_IP
    fi
    
    if [ -z "$user_port" ]; then
        user_port="5555"
    fi
    
    echo "SERVER_IP=$user_ip" >> param.conf
    echo "PORT=$user_port" >> param.conf
    echo "[*] Configuring Client to connect to $user_ip:$user_port..."

else
    # --- STANDALONE MODE ---
    echo "MODE=standalone" >> param.conf
    echo "SERVER_IP=127.0.0.1" >> param.conf
    echo "PORT=5555" >> param.conf
    echo "[*] Starting in STANDALONE mode."
fi

# LAUNCH THE GAME
# Using konsole as per your environment
echo "[*] Launching Simulation..."
konsole -e ./server &

echo "Done. Game running in new window."