CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lcurl -ljansson -lm

TARGET = aircraft_display
SRC = aircraft_display.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
