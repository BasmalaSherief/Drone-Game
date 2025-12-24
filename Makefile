# Compiler and Flags
CC = gcc
CFLAGS = -I. -Wall
LIBS = -lncurses -lm

# Targets
all: server drone keyboard

# 1. Compile Shared Modules
common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c -o common.o

ObstaclesGenerator.o: ObstaclesGenerator/ObstaclesGenerator.c ObstaclesGenerator/ObstaclesGenerator.h
	$(CC) $(CFLAGS) -c ObstaclesGenerator/ObstaclesGenerator.c -o ObstaclesGenerator.o

TargetGenerator.o: TargetGenerator/TargetGenerator.c TargetGenerator/TargetGenerator.h
	$(CC) $(CFLAGS) -c TargetGenerator/TargetGenerator.c -o TargetGenerator.o

Blackboard_functions.o: BlackBoardServer/Blackboard_functions.c BlackBoardServer/Blackboard.h
	$(CC) $(CFLAGS) -c BlackBoardServer/Blackboard_functions.c -o Blackboard_functions.o

# 2. Blackboard Server
# Links: common, obstacles, targets, and blackboard functions
server: BlackBoardServer/BlackboardServer.c common.o ObstaclesGenerator.o TargetGenerator.o Blackboard_functions.o
	$(CC) $(CFLAGS) BlackBoardServer/BlackboardServer.c common.o ObstaclesGenerator.o TargetGenerator.o Blackboard_functions.o -o server $(LIBS)

# 3. Drone Controller
# Links: common and obstacles (for physics)
drone: DroneDynamics/DroneController.c common.o ObstaclesGenerator.o
	$(CC) $(CFLAGS) DroneDynamics/DroneController.c common.o ObstaclesGenerator.o -o drone $(LIBS)

# 4. Keyboard Manager
# Links: common
keyboard: KeyboardManager/KeyboardManager.c common.o
	$(CC) $(CFLAGS) KeyboardManager/KeyboardManager.c common.o -o keyboard $(LIBS)

# Clean up
clean:
	rm -f server drone keyboard *.o
	rm -f /tmp/fifo*