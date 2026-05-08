#include <stdlib.h>
#include <string.h>
#include "../include/parser.h"

int parse_command(const char *buffer, struct command *cmd){
    cmd->argc = 0;

    //check if cmd is an array
    if (*buffer != '*') return 0;

    //total number of commands
    int argc = atoi(buffer + 1);
    if (argc <=0 || argc > MAX_ARGS) return 0;

    //move to the end of first line
    char *ptr = strchr(buffer,'\n');
    if (!ptr) return 0;
    ptr++;

    //parse each argument
    for (int i=0;i<argc;i++){

        //check if command starts with $ (bulk string)
        if (*ptr != '$') return 0;
        int len = atoi(ptr + 1);
        if (len <= 0 || len >= MAX_ARG_LEN) return 0;

        ptr = strchr(ptr,'\n');
        if (!ptr) return 0;
        ptr++;

        //copy command to argv and move to next command
        strncpy(cmd->argv[i],ptr,len);
        cmd->argv[i][len] = '\0';

        //move to the next command
        ptr += len + 2;
        cmd->argc++;
    }

    return 1;
}