#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "hashtable.h"
#define PATH "data/appendonly.aof"

int aof_open(void);
void aof_log_set(char *key,char *value);
void aof_log_expireat(char *key,long timestamp);
void aof_log_del(char *key);
void aof_load(HashTable *ht);
void aof_close(void);

#endif
