#ifndef HASHTABLE_H
#define HASHTABLE_H

#define TABLE_SIZE 64 //number of buckets

//structure for one key-value entry
typedef struct Entry{
    char *key;
    char *value;
    struct Entry *next;
} Entry;

//structure of hashtable
typedef struct Hashtable{
    Entry *buckets[TABLE_SIZE];//array of pointers to entries
} HashTable;

// Function declarations
HashTable* ht_create();
void ht_set(HashTable *ht, const char *key, const char *value);
char* ht_get(HashTable *ht, const char *key);
void ht_delete(HashTable *ht, const char *key);
void ht_free(HashTable *ht);

#endif