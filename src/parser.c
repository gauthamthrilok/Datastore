#include <stdlib.h>
#include <string.h>
#include "../include/parser.h"

//check whether buffer contains a complete resp style command
//return -1 if invalid command, 0 if incomplete command and 1 if complete command
int is_complete(const char* buffer, int buf_len, int* used){
    if (buf_len < 4) return 0;
    if (buffer[0] != '*') return -1;

    //parse argc and check if it is valid
    const char* ptr = buffer;
    const char* end = buffer + buf_len;

    int argc = atoi(ptr + 1);
    if (argc <= 0 || argc > MAX_ARGS) return -1;

    ptr = memchr(ptr,'\n',(size_t)(end-ptr));
    if (!ptr) return 0;
    ptr++;

    //parse through each argument
    for (int i=0;i<argc;i++){
        //check if atleast one $N\r\n is present
        if (ptr >= end) return 0;

        //check if ptr is at '$'
        if (*ptr != '$') return -1;
        int len = atoi(ptr + 1);
        if (len < 0 || len > MAX_ARG_LEN) return -1;

        ptr = memchr(ptr,'\n',(size_t)(end-ptr));
        if (!ptr) return 0;
        ptr++;

        if (ptr + len + 2 > end) return 0;

        ptr += len + 2;
    }

    //Total bytes occupied by this command. Useful when multiple commands are received by single recv command
    *used = (int)(ptr - buffer);
    return 1;

}

int parse_command(const char *buffer, int n, struct command *cmd){
    cmd->argc = 0;
    const char* ptr = buffer;
    const char* end = buffer + n;

    //check if cmd is an array
    if (*buffer != '*') return 0;

    //total number of commands
    int argc = atoi(buffer + 1);
    if (argc <=0 || argc > MAX_ARGS) return 0;

    //move to the end of first line
    ptr = memchr(buffer,'\n',(size_t)(end-ptr));
    if (!ptr) return 0;
    ptr++;

    //parse each argument
    for (int i=0;i<argc;i++){

        //check if command starts with $ (bulk string)
        if (*ptr != '$') return 0;
        int len = atoi(ptr + 1);
        if (len <= 0 || len > MAX_ARG_LEN) return 0;

        ptr = memchr(ptr,'\n',(size_t)(end - ptr));
        if (!ptr) return 0;
        ptr++;

        //copy command to argv and move to next command
        memcpy(cmd->argv[i],ptr,(size_t)len);
        cmd->argv[i][len] = '\0';

        //move to the next command
        ptr += len + 2;
        cmd->argc++;
    }

    return 1;
}
