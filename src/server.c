#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../include/server.h"
#include "../include/hashtable.h"
#include "../include/parser.h"
#include "../include/persistence.h"

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
        aof_log_set(cmd->argv[1],cmd->argv[2]);
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
        aof_log_del(cmd->argv[1]);
        send_simple(client_fd,"+OK\r\n");
        return;
    } 

    if (strcmp("EXPIRE",cmd->argv[0])==0){
        if (cmd->argc < 3){
            send_simple(client_fd, "-ERR too few arguments\r\n");
            return;
        }
        int seconds = atoi(cmd->argv[2]);
        if (seconds <= 0){
            send_simple(client_fd, "-ERR invalid expire time\r\n");
            return;
        }
        ht_expire(ht, cmd->argv[1], seconds);
        long timestamp = time(NULL) + seconds;
        aof_log_expireat(cmd->argv[1], timestamp);
        send_simple(client_fd,"+OK\r\n");
        return;
    }

    if (strcmp("TTL",cmd->argv[0])==0){
        if (cmd->argc < 2){
            send_simple(client_fd, "-ERR too few arguments\r\n");
            return;
        }
        int ttl = ht_ttl(ht, cmd->argv[1]);
        char response[32];
        snprintf(response,sizeof(response),":%d\r\n",ttl);
        send_simple(client_fd, response);
        return;
    }
        
    send_simple(client_fd,"-ERR unknown command\r\n");
    
}

//to handle commands from each client
static int handle_client(client *client,HashTable *ht){
    char chunk[BUFFER_SIZE];
    ssize_t bytes = recv(client->fd,chunk,BUFFER_SIZE,0);

    //client disconnected
    if (bytes<=0){
        //client buffer contains commands which are not parsed yet
        if (client->buf_len > 0){
            send_simple(client->fd,"-ERR protocol error\r\n");
        }
        return 0;
    }

    if (client->buf_len + (int)bytes > BUFFER_SIZE){
        send_simple(client->fd, "-ERR command too large\r\n");
        client->buf_len = 0;
        return 1;
    }

    memcpy(client->buffer + client->buf_len,chunk,(size_t)bytes);
    client->buf_len += (int)bytes;

    while (client->buf_len > 0){
        int used = 0;
        int status = is_complete(client->buffer, client->buf_len, &used);

        if (status == -1){
            send_simple(client->fd,"-ERR protocol error\r\n");
            client->buf_len = 0;
            break;
        }

        //incomplete command
        if (status == 0){
            break;
        }

        command cmd;
        if (parse_command(client->buffer, used, &cmd)){
            handle_command(client->fd, ht, &cmd);
        } else {
            send_simple(client->fd,"-ERR protocol error\r\n");
            break;
        }

        //consume parsed command and move rest of buffer to the front
        memmove(client->buffer,client->buffer+used,(size_t)(client->buf_len-used));
        client->buf_len -= used;
    }

    return 1;
}

void start_server(int port,int max_keys){
    HashTable* ht = ht_create(max_keys); //create a new hashtable
    if (!ht) {
        perror("Hashtable creation failed");
        exit(1);
    }

    aof_load(ht); //load all command from aof

    if (!aof_open()){
        perror("Could not open aof");
        exit(1);
    }

    client clients[MAX_CLIENTS];
    for (int i=0;i<MAX_CLIENTS;i++){
        clients[i].fd = -1;
        clients[i].buf_len = 0;
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
    int count_requests = 0;
    while(1){
        fd_set read_fds;
        FD_ZERO(&read_fds);

        FD_SET(server_fd,&read_fds);
        int max_fd = server_fd;
        
        for (int i=0;i<MAX_CLIENTS;i++){
            if (clients[i].fd != -1){
                FD_SET(clients[i].fd,&read_fds);
                if (clients[i].fd > max_fd){
                    max_fd = clients[i].fd;
                }
            }
        }

        int rv = select(max_fd+1,&read_fds,NULL,NULL,NULL);

        if (rv<0){
            perror("Select failure");
            continue;
        }

        if (FD_ISSET(server_fd,&read_fds)){
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int client_fd = accept(server_fd,(struct sockaddr *)&client_addr,&len);
            if (client_fd < 0){
                perror("Accept failure");
            } else {
                int slot = -1;

                for (int i=0;i<MAX_CLIENTS;i++){
                    if (clients[i].fd == -1){
                        slot = i;
                    }
                }

                if (slot == -1){
                    send_simple(client_fd, "-ERR max clients reached\r\n");
                    close(client_fd);
                } else {
                    clients[slot].fd = client_fd;
                    printf("Client %d connected in slot %d\n",client_fd,slot);
                }
            }
        }

        for (int i=0;i<MAX_CLIENTS;i++){
            if (clients[i].fd != -1){
                if (FD_ISSET(clients[i].fd,&read_fds)){
                    int alive = handle_client(&clients[i], ht);

                    if (!alive){
                        printf("Client %d disconnected from slot %d\n",clients[i].fd,i);
                        close(clients[i].fd);
                        clients[i].fd = -1;
                        clients[i].buf_len = 0;
                    }

                    count_requests++;
                    if (count_requests > 100){
                        ht_rm_expired(ht);
                        count_requests = 0;
                    }
                }
            }
        }
    }

    aof_close();
    close(server_fd);
    ht_free(ht);
}
