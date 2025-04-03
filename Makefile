# Directories
SRC_DIR=src
INCLUDE_DIRS=$(wildcard include/*)
OBJ_DIR=build
INCLUDE_FLAGS=$(addprefix -I,$(INCLUDE_DIRS))

# Compiler and flags
CC=gcc
CFLAGS=$(INCLUDE_FLAGS) -Wall  -g

# Find all .c files in SRC_DIR and its subdirectories
SRCS=$(shell find $(SRC_DIR) -name '*.c')
OBJS=$(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(SRCS:.c=.o))

# Output executable
TARGET=cib

# Default target
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

# Compile .c files to .o files
$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c $(INCLUDE_DIRS)
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)