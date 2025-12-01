# Compiler and Flags
CC = gcc
CFLAGS = -I.
LIBS = -lncurses -lm

# Targets (The files we want to build)
all: server drone keyboard

# 1. Blackboard Server (Map Display & Game Logic)
server: BlackBoardServer/BlackboardServer.c
	$(CC) $(CFLAGS) BlackBoardServer/BlackboardServer.c -o server $(LIBS)

# 2. Drone Controller (Physics Engine)
# We link ObstaclesGenerator.c here because the drone needs the physics math
drone: DroneDynamics/DroneController.c ObstaclesGenerator/ObstaclesGenerator.c
	$(CC) $(CFLAGS) DroneDynamics/DroneController.c ObstaclesGenerator/ObstaclesGenerator.c -o drone $(LIBS)

# 3. Keyboard Manager (Input & Telemetry)
keyboard: KeyboardManager/KeyboardManager.c
	$(CC) $(CFLAGS) KeyboardManager/KeyboardManager.c -o keyboard $(LIBS)

# Clean up command to remove executables and old pipes
clean:
	rm -f server drone keyboard
	rm -f /tmp/fifo*