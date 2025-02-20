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
#define BUCKETS 8

typedef enum {
    INTEGER = 0,
    FLOAT,
    BOOLEAN,
    STRING
} ColumnType;

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
    HashTable index_table;
    ColumnType *type;
    u8 **header;
    Row *rows;
    u64 rows_count;
    u64 cols_count;
} CSV;

/*  
 * ARENA ALLOCATOR IMPLEMENTATION
 */
Region *new_region(size_t capacity);
void free_region(Region *r);
void *arena_alloc(Arena *a, size_t bytes);
void *arena_realloc(Arena *a, void *_oldptr, size_t _oldsize, size_t _newsize);
void arena_reset(Arena *a);
void arena_free(Arena *a);

/*  
 * ARENA ALLOCATOR IMPLEMENTATION
 */

/*  
 * HASHTABLE IMPLEMENTATION
 */
void insert_into_hash(HashTable *table, u8 *key, s32 index, Arena *a);
s32 get_column_index(HashTable *table, const char *key);
/*  
 * HASHTABLE IMPLEMENTATION
 */

/*  
 * CSV IMPLEMENTATION
 */
void init_csv(CSV *csv);
void deinit_csv(CSV *csv);
void print_csv(CSV *csv);
ERRNO read_csv(const char *content, CSV *csv);
u8 **get_column_at(CSV *csv, const u8 *column_name);
ERRNO convert_cell_to_integer(CSV *csv, u32 row, u32 col, s64 *output);
ERRNO convert_cell_to_float(CSV *csv, u32 row, u32 col, double *output);

u64 get_row_count(CSV *csv);
u64 get_col_count(CSV *csv);
u8 **get_header(CSV *csv);
u8 **get_row_at(CSV *csv, u32 idx);

/*  
 * CSV IMPLEMENTATION
 */


