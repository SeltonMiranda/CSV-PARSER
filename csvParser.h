#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * Typedefs for better visualization of data
 */

typedef unsigned long int  u64;
typedef unsigned int       u32;
typedef unsigned short int u16;
typedef unsigned char      u8;

typedef long int  s64;
typedef int       s32;
typedef short int s16;
typedef char      s8;

typedef unsigned char ERRNO; // For different types of errors that might occur

#define REGION_DEFAULT_CAPACITY (8 * 1024)
#define BUCKETS 128

typedef struct Region Region;
typedef struct Region {
    u32 size;
    u32 capacity;
    Region *next;
    uintptr_t data[];
} Region;

typedef struct Arena {
    Region *begin, *end;
} Arena;

typedef struct HashEntry {
    u8 *key;
    size_t index;
    struct HashEntry *next;
} HashEntry;


typedef struct HashTable {
    HashEntry *buckets[BUCKETS];
} HashTable;

typedef struct Row {
    u8 **data;
} Row; 

typedef struct CSV {
    u64 rows_count;
    u64 cols_count;
    Arena allocator;
    HashTable index_table;
    u8 **headers;
    Row *rows;
} CSV;

Region *new_region(size_t capacity);
void free_region(Region *r);
void *arena_alloc(Arena *a, size_t bytes);
void *arena_realloc(Arena *a, void *_oldptr, size_t _oldsize, size_t _newsize);
void arena_reset(Arena *a);
void arena_free(Arena *a);

void insert_into_hash(HashTable *table, char *key, size_t index, Arena *a);
u32 get_column_index(HashTable *table, const char *key);

ERRNO read_csv(const char *content, CSV *csv);
void init_csv(CSV *csv);
void deinit_csv(CSV *csv);
