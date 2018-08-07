.PHONY:all

all=server
server:server.c
	gcc -g -o $@ $^ -pthread
