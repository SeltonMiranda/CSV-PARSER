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
    CSV_TYPE_INTEGER = 0,
    CSV_TYPE_FLOAT,
    CSV_TYPE_BOOLEAN,
    CSV_TYPE_STRING
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
    String_View key;
    size_t index;
    struct HashEntry *next;
} HashEntry;

typedef struct HashTable {
    HashEntry *buckets[BUCKETS];
} HashTable;

typedef struct String_View {
    u8* data;
    u64 size;
} String_View;

typedef struct Row {
    String_View *cells;
} Row; 

typedef struct CSV {
    HashTable index_table;
    u64 cols_count;
    u64 rows_count;
    ColumnType *type;
    String_View *header;
    Row *rows;
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

ERRNO convert_cell_to_integer(CSV *csv, u32 row, u32 col, s64 *output);
ERRNO convert_cell_to_float(CSV *csv, u32 row, u32 col, double *output);
ERRNO save_to_csv(CSV *csv); // TODO

ERRNO convert_column_to_integer(CSV *csv, u32 col, s64 col_output[]);
ERRNO convert_column_to_float(CSV *csv, u32 col, double col_output[]);

// TODO: append a column with at most csv->rows_count rows, if the rows parameter isn't the same as csv->rows_count
// the difference (csv->rows_count - rows) is filled with "NULL"
ERRNO append_column(CSV *csv, u8 **column_to_append, u32 rows);
ERRNO append_many_columns(CSV *csv, u8 ***columns_to_append, u32 rows, u32 cols);
ERRNO append_row(CSV *csv, u8 **row_to_append);
ERRNO append_many_rows(CSV *csv, u8 ***rows_to_append);

// TODO filter, mean, median, mode, treatment of NULL (empty values)

u8 **get_column_at(CSV *csv, const u8 *column_name);
s32 get_column_index(CSV *csv, const char *key);
u64 get_row_count(CSV *csv);
u64 get_col_count(CSV *csv);
u8 **get_header(CSV *csv);
u8 **get_row_at(CSV *csv, u32 idx);
u8 *get_cell(u32 row, const u8 *column_name); // TODO
void csv_filter(CSV *csv, s32 (*predicate)(u32 row)); // TODO


// BRAINSTORMING
// Create CSVRow struct, it would store info about certain row
// Create CSVCol struct, similar to CSVRow
// Function dropna, it removes lines which values are empty

/*  
 * CSV IMPLEMENTATION
 */


