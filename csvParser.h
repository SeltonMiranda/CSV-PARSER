#pragma once

#include <stdint.h>
#include <stddef.h>

// Typedefs

typedef unsigned long int  u64;
typedef unsigned int       u32;
typedef unsigned short int u16;
typedef unsigned char      u8;

typedef long int  s64;
typedef int       s32;
typedef short int s16;
typedef char      s8;

typedef u8 boolean;

// Typedefs


// Defines

#define TRUE 1
#define FALSE 0

#define REGION_DEFAULT_CAPACITY (8 * 1024)
#define BUCKETS 8

#define sv_null (String_View){ .data = NULL, .size = 0 }
#define sv(c_str) (String_View){ .data = c_str, .size = strlen(c_str) }
#define sv_args(str) (s64)(str.size), (str.data)
#define sv_fmt "%.*s"

#define BUFFER_SIZE 8192

#define ALIGNMENT 16  
#define ALIGN_UP(x, a) (((x) + (a - 1)) & ~(a - 1))

// Defines

// Simple error system
typedef enum {
    NIL = 0,
    ERR_MEM_ALLOC,
    ERR_FILE_NOT_FOUND,
    ERR_OPEN_FILE,
    ERR_CSV_EMPTY,
    ERR_EMPTY_CELL,
    ERR_CSV_OUT_OF_BOUNDS,
    ERR_CSV_DIFF_TYPE,
    ERR_COLUMN_NOT_FOUND,
    ERR_INVALID_COLUMN,
    ERR_INVALID_ARG,
    ERR_INCONSISTENT_COLUMNS,
    ERR_UNKNOWN
} ERRNO;

typedef enum {
    CSV_TYPE_INTEGER = 0,
    CSV_TYPE_FLOAT,
    CSV_TYPE_BOOLEAN,
    CSV_TYPE_STRING,
    CSV_TYPE_UNKNOWN
} ColumnType;


//----- Credits to tsoding: https://github.com/tsoding/arena ------
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
// ----------------------------------------------------------------

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
    Arena allocator;
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

/*
 * Checks if an error occured. If NIL, then nothing happened.
 * @return boolean: True if an error occurred else false.
 */

boolean error();

/*
 * Initializes a CSV struct
 * @param csv: struct CSV
 */
void init_csv(CSV *csv);

/*
 * Destroys a CSV struct
 * @param csv: struct CSV
 */
void deinit_csv(CSV *csv);

/*
 * Prints the csv's content formatted. May throws an error.
 * @param csv: struct CSV
 */
void print_csv(CSV *csv);


/*
 * Prints a column. May throws an error.
 * @param column: Array of rows from that column.
 * @param rows: Size of array.
 */
void print_column(const String_View *column, u64 rows);

/*
 * Reads a csv from a file, store its content in a CSV struct,
 * may throw an error.
 * @param content: file path
 * @param csv: Pointer to a CSV struct
 */
void read_csv(const char *content, CSV *csv);

/*
 * Saves the csv's content to a file, if output_file is NULL, saves into a predetermined name.
 * May throw an error.
 * @param output_file: output file path
 * @param csv: Pointer to a CSV struct
 */
void save_csv(const char *output_file, CSV *csv);

/*
 * Checks if a csv is empty.
 * @param csv: Pointer to a CSV struct
 * @return boolean: True wether is empty or false if not
 */
boolean is_csv_empty(CSV *csv);

/*
 * Checks if a csv's cell is empty.
 * @param cell: A string_view of a cell
 * @return boolean: True wether is empty else False
 */

boolean is_cell_empty(String_View cell);

/*
 * Converts a cell's value to integer.
 * @param cell: A string_view of a cell.
 * @return signed integer: Signed value of a cell
 */
s64 to_integer(String_View cell);

/*
 * Converts a cell's value to floating point.
 * @param cell: A string_view of a cell
 * @return double: double value of a cell
 */
double to_float(String_View cell);

/*
 * Convert a cell at specific row and column to integer.
 * @param csv: Pointer to a CSV struct.
 * @param row: Index row.
 * @param col: Index column.
 * @param output: Pointer to a value to be returned.
 */
void convert_cell_to_integer(CSV *csv, u32 row, u32 col, s64 *output);

/*
 * Convert a cell at specific row and column to floating point.
 * @param csv: Pointer to a CSV struct.
 * @param row: Index row.
 * @param col: Index column.
 * @param output: Pointer to a value to be returned.
 */
void convert_cell_to_float(CSV *csv, u32 row, u32 col, double *output);

/*
 * Fills empty values in a csv.
 * @param csv: Pointer to a CSV struct
 */
void fillna(CSV *csv);

/*
 * Removes lines which has empty values. May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @return csv: New CSV struct without empty values.
 */
CSV dropna(CSV *input_csv);


/*
 * Appends an column to a csv, it need to has the same quantity of rows in the csv.
 * Assumes that first row is the header. May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param column_to_append: Array to append.
 * @param rows: Rows of the column that will be append.
 */
void append_column(CSV *csv, String_View *column_to_append, u32 rows);

/*
 * Appends various column to a csv, it need to has the same quantity of rows in the csv.
 * Assumes that first row is the header. May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param columns_to_append: Array of arrays to append.
 * @param rows_to_append: Rows of the column that will be append.
 * @param cols_to_append: How many columns that will append.
 */
void append_many_columns(CSV *csv, String_View **columns_to_append, u32 rows_to_append, u32 cols_to_append);

/*
 * Appends a row to a csv, it need to has the same quantity of columns in the csv.
 * May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param row_to_append: Array to append.
 * @param cols_to_append: cols of the row that will be append.
 */
void append_row(CSV *csv, String_View *row_to_append, u32 cols_to_append);

/*
 * Appends various rows to a csv, it need to has the same quantity of columns in the csv.
 * May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param rows_to_append: Array of arrays to append.
 * @param many_cols: cols of the row that will be append.
 * @param many_rows: How many rows that will append.
 */
void append_many_rows(CSV *csv, String_View **rows_to_append, u32 many_rows, u32 many_cols);

/*
 * Gets the mean from a numeric column. May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param column_name: Name of a column.
 * @param output: Pointer to a value to be returned
 */
void csv_mean(CSV *csv, String_View column_name, double *output);

/*
 * Gets the median from a numeric column. May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param column_name: Name of a column.
 * @param output: Pointer to a value to be returned
 */
void csv_median(CSV *csv, String_View column_name, double *output);


void csv_mode_integer(CSV *csv, String_View column_name, s64 *output); // *TO-DO*, for easy implementation, needs an hashmap
void csv_mode_double(CSV *csv, String_View column_name, s64 *output); // *TO-DO*, for easy implementation, needs an hashmap

/*
 * Gets the standard deviation from a numeric column. May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param column_name: Name of a column.
 * @param output: Pointer to a value to be returned
 */
void csv_sd(CSV *csv, String_View column_name, double *output);
void drop_duplicater(CSV *csv, String_View column_name); // TO-DO

/*
 * Returns the index of a column. 
 * @param key: Column's name.
 * @return signed integer: the column's index or -1 if not found.
 */
s32 get_column_index(String_View *key);

/*
 * Returns the quantity of rows from a csv. 
 * @param csv: Pointer to a CSV struct.
 * @return unsigned integer: quantity of rows.
 */
u64 get_row_count(CSV *csv);

/*
 * Returns the quantity of columns from a csv. 
 * @param csv: Pointer to a CSV struct.
 * @return unsigned integer: quantity of columns.
 */
u64 get_col_count(CSV *csv);

/*
 * Returns a const reference to the header of a csv. May throws an error
 * @param csv: Pointer to a CSV struct.
 * @return: Reference to csv header.
 */
const String_View *get_header(CSV *csv);

/*
 * Returns a const reference of a line in csv. May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param idx: Row index.
 * @return: Reference to a specific row.
 */
const String_View *get_row_at(CSV *csv, u32 idx);

/*
 * Returns a specific column. May thrown an error.
 * @param csv: Pointer to a CSV struct.
 * @param column_name: Column to be returned.
 * @return column: Reference to column.
 */
const String_View *get_column(CSV *csv, String_View column_name);

/*
 * Returns the cell from a row and column. May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param row: Row index.
 * @param column_name: Name of the column.
 * @return: string_view of a cell.
 */
String_View get_cell(CSV *csv, u32 row, String_View *column_name);

/*
 * Filter cells with function parameter. May throws an error.
 * @param csv: Pointer to a CSV struct.
 * @param column_name: Name of the column.
 * @param predicate: Function utilized to filtrate.
 * @param out_count: Pointer to a returned value of number of rows that were filtrated.
 * @return: Array of cells filtrated.
 */
String_View *csv_filter(CSV *csv, String_View column_name, boolean (*predicate)(String_View cell), u64 *out_count);

/*  
 * CSV IMPLEMENTATION
 */


