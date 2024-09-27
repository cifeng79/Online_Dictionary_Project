CC=gcc

all:client server

client:client.c
	$(CC) -o client $^
server:server.c
	$(CC) -o server $^ -lsqlite3

clean:
	rm client server
