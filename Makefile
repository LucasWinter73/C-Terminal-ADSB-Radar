CC = gcc
CFLAGS = -Wall -O2 -D_GNU_SOURCE
LIBS = -lcurl -ljansson -lm

# macOS Homebrew paths (for Apple Silicon and Intel)
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    BREW_PREFIX := $(shell brew --prefix)
    CFLAGS += -I$(BREW_PREFIX)/include
    LIBS += -L$(BREW_PREFIX)/lib
endif

TARGET = aircraft_display_radar
SOURCE = aircraft_display_with_radar.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run