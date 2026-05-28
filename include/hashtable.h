#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <time.h>

#define TABLE_SIZE 64 //number of buckets

//structure for one key-value entry
typedef struct Entry{
    char *key;
    char *value;
    time_t expiry;
    struct Entry *next;
} Entry;

//structure of hashtable
typedef struct Hashtable{
    Entry *buckets[TABLE_SIZE];//array of pointers to entries
} HashTable;

// Function declarations
HashTable* ht_create(void);
void ht_set(HashTable *ht, const char *key, const char *value);
char* ht_get(HashTable *ht, const char *key);
int ht_ttl(HashTable *ht, const char *key);
void ht_expire(HashTable *ht, const char *key, int n);
void ht_expire_at(HashTable *ht, const char *key,long timestamp);
void ht_rm_expired(HashTable *ht);
void ht_delete(HashTable *ht, const char *key);
void ht_free(HashTable *ht);

#endif
