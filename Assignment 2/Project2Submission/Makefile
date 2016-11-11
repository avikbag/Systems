TARGET = mini_grep
LIBS = -lm
CC = gcc
CFLAGS = -std=c99 -g -Wall -pthread

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@ -pthread

clean:
	-rm -f *.o
	-rm -f $(TARGET)
