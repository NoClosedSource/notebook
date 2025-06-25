CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0` -O2 -Wall
LDFLAGS = `pkg-config --libs gtk+-3.0`
PREFIX = /usr
BIN = notebook

all:
	$(CC) $(CFLAGS) -o $(BIN) src/main.cpp $(LDFLAGS)

install:
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN)
