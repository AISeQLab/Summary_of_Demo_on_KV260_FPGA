CC = gcc
CFLAGS = -Wall -g
DFLAGS = -D_DEBUG
LDFLAGS = 
SOURCES = sm3test.c sm3.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = sm3test

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS += $(DFLAGS)
debug: $(EXECUTABLE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
