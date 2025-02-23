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

typedef u8 boolean;
typedef unsigned char ERRNO; // For different types of errors that might occur

#define TRUE 1
#define FALSE 0

#define REGION_DEFAULT_CAPACITY (8 * 1024)
#define BUCKETS 8

#define sv_null (String_View){ .data = NULL, .size = 0 }
#define sv(c_str) (String_View){ .data = c_str, .size = strlen(c_str) }
#define sv_args(str) (s64)(str.size), (str.data)
#define sv_fmt "%.*s"

typedef enum {
    CSV_TYPE_INTEGER = 0,
    CSV_TYPE_FLOAT,
    CSV_TYPE_BOOLEAN,
    CSV_TYPE_STRING,
    CSV_TYPE_UNKNOWN
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

typedef struct String_View {
    u8* data;
    u64 size;
} String_View;

typedef struct HashEntry {
    String_View key;
    size_t index;
    struct HashEntry *next;
} HashEntry;

typedef struct HashTable {
    HashEntry *buckets[BUCKETS];
} HashTable;

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
 * CSV IMPLEMENTATION
 */
void init_csv(CSV *csv);
void deinit_csv(CSV *csv);
void print_csv(CSV *csv);
ERRNO read_csv(const char *content, CSV *csv);
ERRNO save_csv(const char *output_file, CSV *csv);

boolean is_cell_empty(String_View cell);
s64 to_integer(String_View cell);
double to_float(String_View cell);
ERRNO convert_cell_to_integer(CSV *csv, u32 row, u32 col, s64 *output);
ERRNO convert_cell_to_float(CSV *csv, u32 row, u32 col, double *output);
void fillna(CSV *csv);
CSV dropna(CSV *input_csv); // TODO

ERRNO append_column(CSV *csv, String_View *column_to_append, u32 rows);
ERRNO append_many_columns(CSV *csv, String_View **columns_to_append, u32 rows_to_append, u32 cols_to_append);
ERRNO append_row(CSV *csv, String_View *row_to_append, u32 cols_to_append);
ERRNO append_many_rows(CSV *csv, String_View **rows_to_append, u32 many_rows, u32 many_cols);


ERRNO csv_mean(CSV *csv, String_View column_name, double *output);
ERRNO csv_median(CSV *csv, String_View column_name, double *output);
ERRNO csv_mode_integer(CSV *csv, String_View column_name, s64 *output); // for easy implementation, needs an hashmap
ERRNO csv_mode_double(CSV *csv, String_View column_name, s64 *output); // for easy implementation, needs an hashmap
ERRNO csv_sd(CSV *csv, String_View column_name, double *output);

s32 get_column_index(String_View *key);
u64 get_row_count(CSV *csv);
u64 get_col_count(CSV *csv);
const String_View *get_header(CSV *csv);
const String_View *get_row_at(CSV *csv, u32 idx);
String_View get_cell(CSV *csv, u32 row, String_View *column_name);
String_View *csv_filter(CSV *csv, String_View column_name, boolean (*predicate)(String_View cell), u64 *out_count);


// BRAINSTORMING
// Create CSVRow struct, it would store info about certain row
// Create CSVCol struct, similar to CSVRow
// Function dropna, it removes lines which values are empty

/*  
 * CSV IMPLEMENTATION
 */


