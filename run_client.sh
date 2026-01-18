#!/bin/bash
# run_client.sh - Launches Client WITHOUT killing existing processes

echo "========================================"
echo "      DRONE GAME CLIENT LAUNCHER        "
echo "========================================"

# DO NOT CLEANUP PROCESSES (Unlike run.sh)
# This allows running alongside an existing Server.

# COMPILE (Optional, but good to ensure sync)
# make -s 
# (Skipping make to avoid race conditions if server is running)

# CONFIGURE CLIENT
echo "# Auto-generated config by run_client.sh" > param.conf
echo "MODE=client" >> param.conf

read -p "Enter Server IP Address (default 127.0.0.1): " user_ip
if [ -z "$user_ip" ]; then
    user_ip="127.0.0.1"
fi

read -p "Enter Server Port (default 5555): " user_port
if [ -z "$user_port" ]; then
    user_port="5555"
fi

echo "SERVER_IP=$user_ip" >> param.conf
echo "PORT=$user_port" >> param.conf

echo "[*] Launching Client..."
# Launch in new terminal
konsole -e ./server &

echo "Done. Client running."
