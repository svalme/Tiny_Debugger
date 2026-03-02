# Makefile for tinydbg
CC = gcc
CFLAGS = -O2 -Wall -Wextra -g -Iinclude -D_GNU_SOURCE
LDFLAGS =

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

# Source files
SRCS = $(SRC_DIR)/debugger.c $(SRC_DIR)/commands.c $(SRC_DIR)/memory.c $(SRC_DIR)/breakpoint.c $(SRC_DIR)/backtrace.c

# Object files
OBJS = $(OBJ_DIR)/debugger.o $(OBJ_DIR)/commands.o $(OBJ_DIR)/memory.o $(OBJ_DIR)/breakpoint.o $(OBJ_DIR)/backtrace.o

# Target executable
TARGET = tinydbg

# Test program
TEST = test

all: $(OBJ_DIR) $(TARGET) $(TEST)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/debugger.o: $(SRC_DIR)/debugger.c $(INC_DIR)/types.h $(INC_DIR)/memory.h $(INC_DIR)/breakpoint.h $(INC_DIR)/backtrace.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/commands.o: $(SRC_DIR)/commands.c $(INC_DIR)/commands.h $(INC_DIR)/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/memory.o: $(SRC_DIR)/memory.c $(INC_DIR)/memory.h $(INC_DIR)/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/breakpoint.o: $(SRC_DIR)/breakpoint.c $(INC_DIR)/breakpoint.h $(INC_DIR)/memory.h $(INC_DIR)/types.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/backtrace.o: $(SRC_DIR)/backtrace.c $(INC_DIR)/backtrace.h $(INC_DIR)/types.h
	$(CC) $(CFLAGS) -c $< -o $@

# Build test program 
$(TEST): test.c
	$(CC) -g -O0 -fno-omit-frame-pointer -no-pie test.c -o $(TEST)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(TEST)

.PHONY: all clean