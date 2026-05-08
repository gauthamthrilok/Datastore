#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 10
#define MAX_ARG_LEN 512

typedef struct command{
    int argc;
    char argv[MAX_ARGS][MAX_ARG_LEN];
} command;

int parse_command(const char *buffer, command* cmd);

#endif