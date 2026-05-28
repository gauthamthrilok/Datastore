#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/persistence.h"
#include "../include/hashtable.h"

static FILE *aof_file = NULL;

int aof_open(void){
    aof_file = fopen(PATH, "a");
    if (!aof_file){
        perror("aof not open");
        return 0;
    }

    return 1;
}

static void aof_write(const char *line){
    if (!aof_file){
        perror("aof not open");
    }
    fprintf(aof_file,"%s\n",line);
    fflush(aof_file);
}

void aof_log_set(char *key, char *value){
    char line[1024];
    snprintf(line,sizeof(line),"SET %s %s",key,value);
    aof_write(line);
}

void aof_log_expireat(char *key, long timestamp){
    char line[1024];
    snprintf(line,sizeof(line),"EXPIRE %s %ld",key,timestamp);
    aof_write(line);
}

void aof_log_del(char *key){
    char line[256];
    snprintf(line,sizeof(line),"DEL %s",key);
    aof_write(line);
}

void aof_load(HashTable *ht){
    FILE *fptr = fopen(PATH,"r");
    if (!fptr) return;

    char line[1024];
    while (fgets(line,sizeof(line),fptr)){
        line[strcspn(line,"\n")] = '\0';

        char cmd[16],key[256],value[512];
        int args = sscanf(line,"%s %s %s",cmd,key,value);
        if (args<2) continue; //skip the line

        if (strcmp("SET",cmd)==0){
            ht_set(ht, key, value);
        } else if (strcmp("EXPIRE  ",cmd)==0){
            long timestamp = atol(value);
            //store only if not expired
            if (timestamp>(long)time(NULL)){
                ht_expire_at(ht,key,timestamp);
            //else remove the key
            } else {
                ht_delete(ht, key);
            }
        } else if (strcmp("DEL",cmd)==0){
            ht_delete(ht,key);
        }
    }

    fclose(fptr);
    printf("AOF file loaded successfully\n");
}

void aof_close(void){
    if (aof_file){
        fclose(aof_file);
        aof_file = NULL;
    }
}
