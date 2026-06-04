#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/hashtable.h"

//static hash function which generates an index between 0 and 64 for any key
static unsigned int hash(const char *key){
    unsigned int hash = 0;
    while (*key){
        hash = (hash * 31u) + (unsigned char)*key; //ASCII of current character 
        key++;
    }
    return hash % TABLE_SIZE;
}

//to remove a key from doubly-linked list
static void lru_remove(HashTable *ht, Entry *e){
    if (e->lru_prev){
        e->lru_prev->lru_next = e->lru_next;
    } else { //e is the head
        ht->head = e->lru_next;
    }

    if (e->lru_next){
        e->lru_next->lru_prev = e->lru_prev;
    } else { //e is the tail
        ht->tail = e->lru_prev;
    }

    e->lru_next = NULL;
    e->lru_prev = NULL;
}

static void lru_insert(HashTable *ht,Entry *e){
    e->lru_prev = NULL;
    e->lru_next = ht->head;

    if (ht->head){
        ht->head->lru_prev = e;
    }

    ht->head = e;

    if (ht->tail == NULL){
        ht->tail = e;
    }
}

static void move_to_head(HashTable *ht, Entry *e){
    if (ht->head == e) return;
    lru_remove(ht,e);
    lru_insert(ht,e);
}

static void evict(HashTable *ht){
    if (ht->tail == NULL) return;

    printf("Evicting key %s\n",ht->tail->key);
    Entry *victim = ht->tail;
    unsigned int index = hash(victim->key);
    Entry *entry = ht->buckets[index];
    Entry *prev = NULL;

    while (entry != NULL){
        if (entry == victim){
            if (prev == NULL){
                ht->buckets[index] = entry->next;
            } else {
                prev->next = entry->next;
            }
            break;
        }
        prev = entry;
        entry = entry->next;
    }

    lru_remove(ht,victim);
    free(victim->key);
    free(victim->value);
    free(victim);
    ht->count--;
}

//create a hashtable
HashTable* ht_create(int max_keys){
    HashTable *ht = malloc(sizeof(HashTable));
    for (int i=0;i<TABLE_SIZE;i++){
        ht->buckets[i] = NULL;  //init all entries to NULL
    }
    ht->head = NULL;
    ht->tail = NULL;
    ht->count = 0;
    ht->max_count = max_keys;
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
            move_to_head(ht, entry);
            return;
        }
        entry = entry->next;
    }

    if (ht->max_count > 0 && ht->count >= ht->max_count){
        evict(ht);
    }

    //if key is not present
    Entry *new = malloc(sizeof(Entry));
    if (!new) return;
    new->key = malloc(strlen(key)+1);
    new->value = malloc(strlen(value)+1);
    if (!new->key || !new->value) return;
    strcpy(new->key,key);
    strcpy(new->value,value);
    new->lru_next = NULL;
    new->lru_prev = NULL;
    new->expiry = 0;

    //insert new entry at the start of the linked list
    new->next = ht->buckets[index];
    ht->buckets[index] = new;
    lru_insert(ht,new);
    ht->count++;
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
                lru_remove(ht,entry);
                free(entry->key);
                free(entry->value);
                free(entry);
                ht->count--;
                return NULL;
            }
            move_to_head(ht,entry);
            return entry->value;
        }
        prev = entry;
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
            lru_remove(ht,entry);
            ht->count--;
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

//set expiry for commands from aof file
void ht_expire_at(HashTable *ht, const char *key, long timestamp){
    unsigned int index = hash(key);
    Entry *entry = ht->buckets[index];
    while (entry != NULL){
        if (strcmp(entry->key,key)==0){
            entry->expiry = (time_t)timestamp;
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
                lru_remove(ht,entry);
                ht->count--;
                free(entry->key);
                free(entry->value);
                free(entry);
            } else {
                prev = entry;
            }
            entry = next;
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
