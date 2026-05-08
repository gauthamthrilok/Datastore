#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../include/server.h"
#include "../include/hashtable.h"
#include "../include/parser.h"

//check whether buffer contains a complete resp style command
int is_complete(char* buffer, int buf_len){
    if (buf_len < 4) return 0;
    if (buffer[0] != '*') return 0;

    //parse argc and check if it is valid
    char* ptr = buffer;
    int argc = atoi(ptr + 1);
    if (argc <= 0) return 0;

    ptr = memchr(ptr,'\n',buffer + buf_len - ptr);
    if (!ptr) return 0;
    ptr++;

    //parse through each argument
    for (int i=0;i<argc;i++){
        //check if atleast one $N\r\n is present
        if (ptr >= buffer + buf_len) return 0;

        //check if ptr is at '$'
        if (*ptr != '$') return 0;
        int len = atoi(ptr + 1);
        if (len < 0) return 0;

        ptr = memchr(ptr,'\n',buffer + buf_len - ptr);
        if (!ptr) return 0;
        ptr++;

        if (ptr + len + 2 > buffer + buf_len) return 0;

        ptr += len + 2;
    }

    return 1;

}

//to send simple string
void send_simple(int client_fd,const char* msg){
    send(client_fd,msg,strlen(msg),0);
}

//to send bulk string
void send_bulk(int client_fd,const char* msg){
    if (msg == NULL){
        send_simple(client_fd,"$-1\r\n");
        return;
    }
    char header[32];
    snprintf(header,sizeof(header),"$%zu\r\n",strlen(msg));
    send_simple(client_fd,header);
    send_simple(client_fd, msg);
    send_simple(client_fd,"\r\n");
}

void handle_command(int client_fd,HashTable* ht,command* cmd){
    if (cmd->argc == 0){
        send_simple(client_fd,"-ERR empty command\r\n");
        return;
    }

    if (strcmp("SET",cmd->argv[0])==0){
        if (cmd->argc < 3){
            send_simple(client_fd,"-ERR too few arguments\r\n");
            return;
        }
        ht_set(ht, cmd->argv[1], cmd->argv[2]);
        send_simple(client_fd,"+OK\r\n");
        return;
    }
    
    if (strcmp("GET",cmd->argv[0])==0) {
        if (cmd->argc < 2){
            send_simple(client_fd,"-ERR too few arguments\r\n");
            return;
        }
        char* val = ht_get(ht,cmd->argv[1]);
        send_bulk(client_fd,val);
        return;
    }
    
    if (strcmp("DEL",cmd->argv[0])==0){
        if (cmd->argc < 2){
            send_simple(client_fd, "-ERR too few arguments\r\n");
            return;
        }
        ht_delete(ht,cmd->argv[1]);
        send_simple(client_fd,"+OK\r\n");
        return;
    } 
        
    send_simple(client_fd,"-ERR unknown command\r\n");
    
}

void start_server(int port){
    HashTable* ht = ht_create(); //create a new hashtable

    //create server socket
    int server_fd = socket(AF_INET,SOCK_STREAM,0);
    if (server_fd<0){
        perror("Socket creation failure\n");
        exit(1);
    }

    //Allow port reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    //bind server socket with server address
    if (bind(server_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
        perror("Bind failure\n");
        exit(1);
    }

    //listen
    if (listen(server_fd,5)<0){
        perror("Listen failure\n");
        exit(1);
    }

    printf("Datastore listening on port : %d\n", port);

    //accept clients
    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd,(struct sockaddr*)&client_addr,&client_len);
        if (client_fd<0){
            perror("Accept failure\n");
            exit(1);
        }

        printf("Client connected\n");

        //init client
        client client;
        client.fd = client_fd;
        client.buf_len = 0;

        //read commands from client
        char chunk[BUFFER_SIZE];
        int bytes;

        while ((bytes=recv(client_fd,chunk,BUFFER_SIZE-1,0))>0){
            //reject command if buffer overflows
            if ((client.buf_len + bytes) > BUFFER_SIZE){
                send_simple(client_fd, "-ERR command too long\r\n");
                client.buf_len = 0;
                continue;
            }
            
            //write chunk of command into buffer
            memcpy(client.buffer + client.buf_len,chunk,bytes);
            client.buf_len += bytes;
            client.buffer[client.buf_len] = '\0';

            //parse command when it is complete
            if (is_complete(client.buffer, client.buf_len)){
                command cmd;
                if (parse_command(client.buffer, &cmd)){
                    handle_command(client_fd, ht, &cmd);
                } else {
                    send_simple(client_fd, "-ERR protocol error\r\n");
                }
                client.buf_len = 0;
            } 
        }

        close(client_fd);
        printf("Client disconnected\n");
    }

    close(server_fd);
    ht_free(ht);
}