#ifndef SERVER_H
#define SERVER_H

#define BUFFER_SIZE 4096

typedef struct client{
    int fd;
    char buffer[BUFFER_SIZE];
    int buf_len;
} client;

void start_server(int port);

#endif