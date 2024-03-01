# Andrada-Ioana Cojocaru 322CA
CC = g++
CFLAGS = -Wall -Wextra -std=c++11 -g
LDFLAGS = -lm

build: server subscriber

run-server:
	./server

run-subscriber:
	./subscriber

server : server.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

subscriber : subscriber.cpp
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -rf subscriber server
.PHONY: build clean