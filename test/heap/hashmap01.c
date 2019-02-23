//@expect fail
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

size_t hash_value(void* ptr);

typedef struct _ListNode {
    struct _ListNode *prev;
    struct _ListNode *next;

    void *data;
} ListNode;

typedef void (*List_dtor_func_t)(void *);

typedef struct _List {
    ListNode *first;
    ListNode *last;

    size_t count;
    List_dtor_func_t dtor;
} List;

List *List_create(List_dtor_func_t dtor);

void List_append(List *list, void *data);
void List_prepend(List *list, void *data);

void List_clean(List *list);
void List_destroy(List *list);

#define LIST_FOREACH(L, S, M, V) ListNode *_node = NULL; \
    ListNode *V = NULL; \
    for (V = _node = L->S; _node != NULL; V = _node = _node->M)

List *List_create(List_dtor_func_t dtor)
{
    List *list = malloc(sizeof(List));

    if (list == NULL) {
        return NULL;
    }

    list->count = 0;
    list->dtor  = dtor;

    list->first = NULL;
    list->last  = NULL;

    return list;
}

void List_append(List *list, void *data)
{
    ListNode *node = malloc(sizeof(ListNode));

    if (node == NULL) {
        return;
    }

    node->data = data;
    node->prev = list->last;
    node->next = NULL;

    if (list->last != NULL) {
        list->last->next = node;
    } else {
        list->first = node;
    }

    list->last = node;
    list->count++;
}

void List_prepend(List *list, void *data)
{
    ListNode *node = malloc(sizeof(ListNode));

    if (node == NULL) {
        return;
    }

    node->data = data;
    node->next = list->first;
    node->prev = NULL;

    if (list->first != NULL) {
        list->first->prev = node;
    } else {
        list->last = node;
    }

    list->first = node;
    list->count++;
}

void List_clean(List *list)
{
    if (list == NULL || list->dtor == NULL) {
        return;
    }

    LIST_FOREACH(list, first, next, curr) {
        list->dtor(curr->data);
    }
}

void List_destroy(List *list)
{
    if (list == NULL) {
        return;
    }

    LIST_FOREACH(list, first, next, curr) {
        if (curr->prev) {
            free(curr->prev);
        }
    }

    free(list->last);
    free(list);
}

typedef struct _HashTableNode {
    uint32_t hash;
    const char *key;

    void *data;
} HashTableNode;

typedef void (*HashTable_dtor_func_t)(void *);

typedef struct _HashTable {

    size_t size;

    // This is an array of linked lists.
    List **buckets;

    HashTable_dtor_func_t dtor;

} HashTable;

HashTable *HashTable_create(size_t size);

uint32_t HashTable_hash(HashTable *ht, const char *key);

bool HashTable_set(HashTable *ht, const char *key, void *data);
void *HashTable_get(HashTable *ht, const char *key);

void HashTable_destroy(HashTable *ht);

HashTable *HashTable_create(size_t size)
{
    HashTable *ht = malloc(sizeof(HashTable));

    ht->size    = size;
    ht->buckets = calloc(ht->size, sizeof(List));
    ht->dtor    = free;

    return ht;
}

static inline List *HashTable_find_bucket(HashTable *ht, uint32_t hash, bool create)
{
    int bucket_n  = hash % ht->size;

    List *bucket  = ht->buckets[bucket_n];

    if (bucket == NULL && create) {
        bucket = List_create(free);
        ht->buckets[bucket_n] = bucket;
    }

    return bucket;
}

bool HashTable_set(HashTable *ht, const char *key, void *data)
{
    uint32_t hash = HashTable_hash(ht, key);
    List *bucket = HashTable_find_bucket(ht, hash, true);

    HashTableNode *node = malloc(sizeof(HashTableNode));

    if (node == NULL) {
        return false;
    }

    node->hash  = hash;
    node->key   = key;
    node->data  = data;

    List_append(bucket, node);

    return true;
}

void *HashTable_get(HashTable *ht, const char *key)
{
    uint32_t hash = HashTable_hash(ht, key);
    List *bucket = HashTable_find_bucket(ht, hash, false);

    if (bucket == NULL) { // No such bucket
        return NULL;
    }

    LIST_FOREACH(bucket, first, next, curr) {
        HashTableNode *node = (HashTableNode*) curr->data;
        if (node->hash == hash && strcmp(node->key, key) == 0) {
            return node->data;
        }
    }

    return NULL;
}

void HashTable_destroy(HashTable *ht)
{
    int i;

    for (i = 0; i < ht->size; i++) {
        List *bucket = ht->buckets[i];

        if (bucket == NULL) {
            continue;
        }

        LIST_FOREACH(bucket, first, next, curr) {
            HashTableNode *node = (HashTableNode*) curr->data;
            ht->dtor(node->data);
        }

        List_destroy(bucket);
    }

    free(ht->buckets);
    free(ht);
}

uint32_t HashTable_hash(HashTable *ht, const char *key)
{
    uint32_t hash, i;
    size_t len = strlen(key);

    for (hash = i = 0; i < len; ++i)
    {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

int main(void)
{
    HashTable* ht = HashTable_create(10);
    
    int x = 10;
    int *p = malloc(sizeof(int));
    *p = 5;

    HashTable_set(ht, "foo", "foo_value");
    HashTable_set(ht, "bar", &x);
    HashTable_set(ht, "baz", p);

    free(p);

    HashTable_destroy(ht);
}
