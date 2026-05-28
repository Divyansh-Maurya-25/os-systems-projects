# Makefile for rush shell project
# Name: Divyansh Maurya
# NetID: U11865935
#
# This Makefile compiles the rush shell program
# Just type "make" to build the executable
# Type "make clean" to remove the executable and object files

# Compiler to use
CC = gcc

# Compiler flags
# -Wall: show all warnings
# -Werror: treat warnings as errors
# -g: include debugging info
CFLAGS = -Wall -Werror -g

# Name of the final executable
TARGET = rush

# Source files
SRCS = rush.c

# Object files (automatically generated from source files)
OBJS = $(SRCS:.c=.o)


# Default target - builds the rush executable
# This runs when you just type "make"
all: $(TARGET)

# Rule to build the rush executable from object files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)
	@echo "Build successful! Run with ./rush"

# Rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up compiled files
# Run with "make clean"
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Cleaned up object files and executable"

# Phony targets (not actual files)
.PHONY: all clean