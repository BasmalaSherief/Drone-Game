#!/bin/bash

echo "--- DRONE GAME LAUNCHER ---"

# 1. BUILD (Satisfies "Build" & "Installation")
echo "[*] Cleaning previous build..."
make clean
echo "[*] Building project..."
make
if [ $? -ne 0 ]; then
    echo "[!] Build failed. Exiting."
    read -p "Press enter to exit..."
    exit 1
fi
echo "[*] Build successful."

# 2. CLEANUP
echo "[*] Cleaning up old pipes..."
rm -f /tmp/fifo*

# 3. CLEAR LOG FILE
echo "[*] Clearing log file..."
> simulation.log 

# 4. LAUNCH BLACKBOARD SERVER
# We capture the Process ID ($!) of the new Konsole window
echo "[*] Launching Blackboard Server..."
konsole -e ./server & 
SERVER_PID=$!

echo "--- SYSTEM RUNNING ---"
echo "Game is active. This terminal will close when the game ends."

# 5. WAIT FOR GAME TO FINISH
wait $SERVER_PID

# 6. CLOSE THIS TERMINAL
echo "Game finished. Closing terminal..."
kill -9 $PPID