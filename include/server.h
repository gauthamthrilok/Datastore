#ifndef SERVER_H
#define SERVER_H
#include "../include/parser.h"

#define BUFFER_SIZE 4096
#define MAX_CLIENTS 64

typedef struct {
    int fd;
    char buffer[BUFFER_SIZE];
    int buf_len;
} client;

void start_server(int port, int max_keys);

#endif
