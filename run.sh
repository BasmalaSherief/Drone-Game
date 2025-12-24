#!/bin/bash

echo "--- DRONE GAME LAUNCHER ---"

# 1. CLEANUP
echo "[*] Cleaning up old pipes..."
rm -f /tmp/fifo*

# 2. CLEAR LOG FILE
echo "[*] Clearing log file..."
> simulation.log 

# 3. LAUNCH BLACKBOARD SERVER
# We capture the Process ID ($!) of the new Konsole window
echo "[*] Launching Blackboard Server..."
konsole -e ./server & 
SERVER_PID=$!

echo "--- SYSTEM RUNNING ---"
echo "Game is active. This terminal will close when the game ends."

# 4. WAIT FOR GAME TO FINISH
wait $SERVER_PID

# 5. CLOSE THIS TERMINAL
echo "Game finished. Closing terminal..."
kill -9 $PPID