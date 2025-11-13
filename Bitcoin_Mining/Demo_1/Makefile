# Compiler and flags
CC = gcc
CFLAGS = 
LDFLAGS = -lcurl -ljansson

# Target executable
TARGET = verification

# Source files
SRCS = main.c

# Build rule
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

# Clean rule
clean:
	rm -f $(TARGET)
