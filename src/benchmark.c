#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>

#define PORT 6379
#define IP "127.0.0.1"
#define NUM_OPS 10000
#define BUFFER_SIZE 4096

//return the current time in micro-seconds
static long curr_time(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long)tv.tv_sec * 1000000L + tv.tv_usec;
}

static int connect_server(void){
    int fd = socket(AF_INET,SOCK_STREAM,0);
    if (fd<0){
        perror("Socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(IP);

    if (connect(fd,(struct sockaddr *)&addr,sizeof(addr))<0){
        perror("Connect");
        exit(1);
    }

    return fd;
}

static int g_fd = -1;

static void ensure_connected(void){
    if (g_fd>=0) return;
    g_fd = connect_server();

}

//to send a buffer and to receive a response
static void send_rcv(int sockfd, const char *buf, size_t len,char *resp){

    (void)sockfd;
    ensure_connected();
    size_t n = send(g_fd,buf,len,0);
    if (n<=0){
        perror("Send failed");
        close(g_fd);
        g_fd = -1;
        return;
    }
    
    size_t m = recv(g_fd,resp,BUFFER_SIZE-1,0);
    if (m<=0){
        perror("Recv failure");
        close(g_fd);
        g_fd = -1;
        return;
    }
    resp[m] = '\0';

    if (resp[0] == '-'){
        printf("Server error : %s",resp);
    }
}

//to build a RESP style SET command
static int build_set(char *buf,int i){
    char key[32],value[32];
    snprintf(key,sizeof(key),"key%d",i);
    snprintf(value,sizeof(value),"val%d",i);

    return snprintf(buf,BUFFER_SIZE,"*3\r\n$3\r\nSET\r\n$%zu\r\n%s\r\n$%zu\r\n%s\r\n",strlen(key),key,strlen(value),value);
}

//to build a RESP style GET command
static int build_get(char *buf,int i){
    char key[32];
    snprintf(key,sizeof(key),"key%d",i);

    return snprintf(buf,BUFFER_SIZE,"*2\r\n$3\r\nGET\r\n$%zu\r\n%s\r\n",strlen(key),key);
}

//run tests
static void run_bench(char *label,int (*build)(char*,int),int fd,int n){
    char cmd[BUFFER_SIZE],resp[BUFFER_SIZE];

    long start = curr_time();
    printf("Blocked");
    for (int i=0;i<n;i++){
        int len = build(cmd,i);
        send_rcv(fd, cmd, (size_t)len, resp);
    }

    long end = curr_time();
    long elapsed_us = end-start;
    double elapsed_s = (double)elapsed_us/1000000.0;
    double ops_sec = (double)n/elapsed_s;
    double lat_ms    = (double)elapsed_us / (double)n / 1000.0;

    printf("%-10s %6d ops | %8.0f ops/sec | %.3f ms/op\n",
           label, n, ops_sec, lat_ms);

}

int main(void){

    signal(SIGPIPE,SIG_IGN);

    printf("Connecting to %s:%d\n", IP, PORT);

    int g_fd = connect_server();
    printf("Connected. Running %d operations each.\n\n", NUM_OPS);

    printf("%-10s %6s     %12s   %s\n",
           "Operation", "Count", "Throughput", "Latency");
    printf("─────────────────────────────────────────────\n");

    run_bench("SET", build_set,g_fd, NUM_OPS);
    run_bench("GET", build_get,g_fd, NUM_OPS);

    close(g_fd);
    return 0;
}
