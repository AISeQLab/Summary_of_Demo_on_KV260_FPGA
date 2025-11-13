# Compiler and flags
CC = gcc
CFLAGS =
LDFLAGS = -lcurl -lssl -lcrypto -lcjson -lm

# Target
TARGET = main

# Source
SRCS = main.c

# Default rule: build and run
all: $(TARGET)

# Build target
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

# Clean target
clean:
	rm -f $(TARGET)
