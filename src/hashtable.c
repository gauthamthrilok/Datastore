#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/hashtable.h"

//static hash function which generates an index between 0 and 64 for any key
static unsigned int hash(const char *key){
    int hash = 0;
    while (*key){
        hash = (hash*31) + *key; //ASCII of current character 
        key++;
    }
    return hash % TABLE_SIZE;
}

//create a hashtable
HashTable* ht_create(void){
    HashTable *ht = malloc(sizeof(HashTable));
    for (int i=0;i<TABLE_SIZE;i++){
        ht->buckets[i] = NULL;  //init all entries to NULL
    }
    return ht;
}

static int is_expired(Entry *entry){
    if (entry->expiry == 0) return 0; //no expiry added
    return time(NULL) > entry->expiry; //check whether current time has exceeded expiry time
}

//add a new key-value entry into the hash-table
void ht_set(HashTable *ht, const char *key, const char *value){
    unsigned int index = hash(key);
    Entry *entry = ht->buckets[index];

    //if key already exists, update value
    while(entry!=NULL){
        if (strcmp(entry->key,key)==0){
            free(entry->value);
            entry->value = malloc(strlen(value)+1);
            strcpy(entry->value,value);
            entry->expiry = 0;
            return;
        }
        entry = entry->next;
    }

    //if key is not present
    Entry *new = malloc(sizeof(Entry));
    new->key = malloc(strlen(key)+1);
    new->value = malloc(strlen(value)+1);
    strcpy(new->key,key);
    strcpy(new->value,value);

    //insert new entry at the start of the linked list
    new->next = ht->buckets[index];
    ht->buckets[index] = new;
}

//lookup key
char* ht_get(HashTable *ht, const char *key){
    unsigned int index = hash(key);
    Entry* entry = ht->buckets[index];
    Entry* prev = NULL;

    while(entry!=NULL){
        Entry* next = entry->next;
        if (strcmp(entry->key,key)==0){
            //lazy expiry
            if (is_expired(entry)){
                if (prev == NULL){
                    ht->buckets[index] = next;
                } else {
                    prev->next = next;
                }
                free(entry->key);
                free(entry->value);
                free(entry);
                return NULL;
            }
            return entry->value;
        }
        prev = entry->next;
        entry = entry->next;
    }
    return NULL; //key not found
}

//deleting a key
void ht_delete(HashTable *ht, const char *key){
    unsigned int index = hash(key);
    Entry* entry = ht->buckets[index];
    Entry* prev = NULL;

    while(entry!=NULL){
        if (strcmp(entry->key,key)==0){
            if (prev==NULL){
                ht->buckets[index] = entry->next; //first entry is to be deleted
            } else {
                prev->next = entry->next;
            }
            free(entry->key);
            free(entry->value);
            free(entry);
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

//set expiry time for a key
void ht_expire(HashTable *ht, const char *key, int n){
    unsigned int index = hash(key);
    Entry *entry = ht->buckets[index];
    while (entry != NULL){
        if (strcmp(entry->key,key)==0){
            entry->expiry = time(NULL) + n;
            return;
        }
        entry = entry->next;
    }
}

//scan hashtable and remove all expired keys
void ht_rm_expired(HashTable *ht){
    for (int i=0;i<TABLE_SIZE;i++){
        Entry *entry = ht->buckets[i];
        Entry *prev = NULL;
        while (entry != NULL){
            Entry *next = entry->next; 
            if (is_expired(entry)){
                if (prev == NULL){
                    ht->buckets[i] = next;
                } else {
                    prev->next = next;
                }
                free(entry->key);
                free(entry->value);
                free(entry);
            } else {
                prev = entry;
            }
            entry = entry->next;
        }
    }
}

int ht_ttl(HashTable *ht, const char *key){
    unsigned int index = hash(key);
    Entry *entry = ht->buckets[index];

    while(entry != NULL){
        if (strcmp(entry->key,key)==0){
            if (is_expired(entry)) return -2; //key not found
            if (entry->expiry == 0) return -1; //expiry not set
            return entry->expiry - time(NULL);
        }
        entry = entry->next;
    }
    return -2;
}
//Free entire hash-table
void ht_free(HashTable *ht){
    for (int i=0;i<TABLE_SIZE;i++){
        Entry* entry = ht->buckets[i];
        while (entry != NULL){
            Entry* next = entry->next;
            free(entry->key);
            free(entry->value);
            free(entry);
            entry = next;
        }
    }
    free(ht);
}
