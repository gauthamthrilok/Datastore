CC     = gcc
CFLAGS = -Wall -Wextra -Wpedantic -g

SRC = src/main.c src/server.c src/parser.c src/hashtable.c src/persistence.c
OBJ = $(SRC:.c=.o)

datastore: $(OBJ)
	$(CC) $(CFLAGS) -o datastore $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) datastore