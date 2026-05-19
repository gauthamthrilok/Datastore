#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../include/server.h"
#include "../include/hashtable.h"
#include "../include/parser.h"

//send commands as a single chunk
static void send_all(int fd, const char* buffer, size_t len){
    while (len>0){
        ssize_t n = send(fd,buffer,len,0);
        if (n<=0) return;
        buffer += n;
        len -= (size_t)(n);
    }
}

//to send simple string
void send_simple(int client_fd,const char* msg){
    send_all(client_fd,msg,strlen(msg));
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
    if (!ht) {
        perror("Hashtable creation failed");
        exit(1);
    }

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

        char buffer[BUFFER_SIZE];
        int buf_len = 0;

        char chunk[BUFFER_SIZE];
        int bytes;

        while ((bytes=recv(client_fd,chunk,BUFFER_SIZE-1,0))>0){
            //reject command if buffer overflows
            if ((buf_len + bytes) > BUFFER_SIZE){
                send_simple(client_fd, "-ERR command too long\r\n");
                buf_len = 0;
                continue;
            }
            
            //write chunk of command into buffer
            memcpy(buffer + buf_len,chunk,bytes);
            buf_len += bytes;
            
            while (buf_len > 0){
                int used = 0;

                int status = is_complete(buffer, buf_len, &used);

                //incomplete command, wait for more chunks
                if (status == 0) break;

                //invalid command
                if (status == -1){
                    send_simple(client_fd, "-ERR protocol error\r\n");
                    buf_len = 0;
                    break;
                }

                command cmd;
                if (parse_command(buffer, used, &cmd)){
                    handle_command(client_fd, ht, &cmd);
                } else {
                    send_simple(client_fd, "-ERR protocol error\r\n");
                }

                // Consume the parsed command from the buffer
                // shift remaining bytes to the front
                memmove(buffer, buffer + used, (size_t)(buf_len - used));
                buf_len -= used;

            }
        }

        //unparsed data leftover in buffer
        if (buf_len > 0) {
            send_simple(client_fd, "-ERR protocol error\r\n");
        }

        close(client_fd);
        printf("Client disconnected\n");
    }

    close(server_fd);
    ht_free(ht);
}