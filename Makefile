# Compiler and Flags
CC = gcc
CFLAGS = -I. -Wall
LIBS = -lncurses -lm

# Targets
all: server drone keyboard obstacle_process target_process watchdog

# ----------------------------
# 1. SHARED MODULES (Functions)
# ----------------------------

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c -o common.o

Obstacles_functions.o: ObstaclesGenerator/Obstacles_functions.c ObstaclesGenerator/ObstaclesGenerator.h
	$(CC) $(CFLAGS) -c ObstaclesGenerator/Obstacles_functions.c -o Obstacles_functions.o

Targets_functions.o: TargetGenerator/Targets_functions.c TargetGenerator/TargetGenerator.h
	$(CC) $(CFLAGS) -c TargetGenerator/Targets_functions.c -o Targets_functions.o

Keyboard_functions.o: KeyboardManager/Keyboard_functions.c KeyboardManager/KeyboardManager.h
	$(CC) $(CFLAGS) -c KeyboardManager/Keyboard_functions.c -o Keyboard_functions.o

Blackboard_functions.o: BlackBoardServer/Blackboard_functions.c BlackBoardServer/Blackboard.h
	$(CC) $(CFLAGS) -c BlackBoardServer/Blackboard_functions.c -o Blackboard_functions.o

NetworkManager.o: NetworkManager/NetworkManager.c NetworkManager/NetworkManager.h
	$(CC) $(CFLAGS) -c NetworkManager/NetworkManager.c -o NetworkManager.o

# ----------------------------
# 2. EXECUTABLES
# ----------------------------

server: BlackBoardServer/BlackboardServer.c common.o Blackboard_functions.o NetworkManager.o
	$(CC) $(CFLAGS) BlackBoardServer/BlackboardServer.c common.o Blackboard_functions.o NetworkManager.o -o server $(LIBS)

drone: DroneDynamics/DroneController.c common.o Obstacles_functions.o
	$(CC) $(CFLAGS) DroneDynamics/DroneController.c common.o Obstacles_functions.o -o drone $(LIBS)

keyboard: KeyboardManager/KeyboardManager.c common.o Keyboard_functions.o
	$(CC) $(CFLAGS) KeyboardManager/KeyboardManager.c common.o Keyboard_functions.o -o keyboard $(LIBS)

obstacle_process: ObstaclesGenerator/ObstaclesGenerator.c common.o Obstacles_functions.o
	$(CC) $(CFLAGS) ObstaclesGenerator/ObstaclesGenerator.c common.o Obstacles_functions.o -o obstacle_process $(LIBS)

target_process: TargetGenerator/TargetGenerator.c common.o Targets_functions.o
	$(CC) $(CFLAGS) TargetGenerator/TargetGenerator.c common.o Targets_functions.o -o target_process $(LIBS)

watchdog: Watchdog/Watchdog.c common.o
	$(CC) $(CFLAGS) Watchdog/Watchdog.c common.o -o watchdog $(LIBS)

# Clean up
clean:
	rm -f server drone keyboard obstacle_process target_process watchdog *.o
	rm -f /tmp/fifo*