#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 10
#define MAX_ARG_LEN 512
#define BUFFER_SIZE 4096

typedef struct command{
    int argc;
    char argv[MAX_ARGS][MAX_ARG_LEN];
} command;

int is_complete(const char *buf, int buf_len, int *used);

int parse_command(const char *buffer, int n, command* cmd);

#endif