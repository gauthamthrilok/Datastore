CC     = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g

SRC = src/main.c src/server.c src/parser.c src/hashtable.c src/persistence.c
OBJ = $(SRC:.c=.o)

all: datastore benchmark

datastore: $(OBJ)
	$(CC) $(CFLAGS) -o datastore $(OBJ)

benchmark: src/benchmark.c
	$(CC) $(CFLAGS) -o benchmark src/benchmark.c

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) datastore benchmark