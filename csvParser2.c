#include "csvParser.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define ALIGNMENT 16  
#define ALIGN_UP(x, a) (((x) + (a - 1)) & ~(a - 1))
#define BUCKETS 8

static Arena tempArena = {0};
static HashTable indexTable = {0};

// Begin Arena

Region *new_region(size_t capacity)
{
    size_t bytes = sizeof(Region) + (sizeof(uintptr_t) * capacity);
    Region *region = (Region *)malloc(bytes);
    if (region == NULL)
    {
        return NULL;
    }
    region->next = NULL;
    region->size = 0;
    region->capacity = capacity;
    return region;
}

void free_region(Region *r)
{
    free(r);
}

void *arena_alloc(Arena *a, size_t bytes)
{
    size_t aligned_size = ALIGN_UP(bytes, ALIGNMENT);
    if (a->end == NULL)
    {
        assert(a->begin == NULL);
        size_t capacity = REGION_DEFAULT_CAPACITY;
        if (capacity < aligned_size)
        {
            capacity = aligned_size;
        } 
        a->end = new_region(capacity);
        if (a->end == NULL)
        {
            return NULL;
        }
        a->begin = a->end;
    }

    while (a->end->size + aligned_size > a->end->capacity && a->end->next != NULL)
    {
        a->end = a->end->next;
    }

    if (a->end->size + aligned_size > a->end->capacity) 
    {
        assert(a->end->next == NULL);
        size_t capacity = REGION_DEFAULT_CAPACITY;
        if (capacity < aligned_size)
        {
            capacity = aligned_size;
        } 
        a->end->next = new_region(capacity);
        if (a->end->next == NULL)
        {
            return NULL;
        }
        a->end = a->end->next;
    }

    void *alloced_bytes = &a->end->data[a->end->size];
    a->end->size += aligned_size;
    return alloced_bytes;
}

void *arena_realloc(Arena *a, void *_oldptr, size_t _oldsize, size_t _newsize)
{
    if (_newsize <= _oldsize)
    {
        return _oldptr;
    }

    void *_newptr = arena_alloc(a, _newsize);
    if (_newptr == NULL)
    {
        return NULL;
    }

    s8 *_newptr_char = _newptr;
    s8 *_oldptr_char = _oldptr;
    memcpy(_newptr, _oldptr, _oldsize); 
    return _newptr;
}

void arena_reset(Arena *a)
{
    for (Region *r = a->begin; r != NULL; r = r->next)
    {
        r->size = 0;
    }
    a->end = a->begin;
}

void arena_free(Arena *a)
{
    Region *r = a->begin;
    while (r != NULL)
    {
        Region *_r = r;
        r = r->next;
        free_region(_r);
    }
    a->begin = NULL;
    a->end = NULL;
}

// End Arena

// Begin HashTable

static u64 hash_function(String_View *str) {
    u64 hash = 5381;
    int c = 0;
    while (c < str->size)
    {
        hash = ((hash << 5) + hash) + str->data[c];
        c++;
    }
    return hash % BUCKETS;
}

static void insert_into_hash(String_View *key, s32 index)
{
   u64 h = hash_function(key);
   HashEntry *new_entry = arena_alloc(&tempArena, sizeof(HashEntry));
   new_entry->key = *key;
   new_entry->index = index;
   new_entry->next = indexTable.buckets[h];
   indexTable.buckets[h] = new_entry;
}

s32 get_column_index(String_View *key)
{
   u64 h = hash_function(key);
   HashEntry *entry = indexTable.buckets[h];
   while (entry) 
   {
        if (
            entry->key.size == key->size &&
            strncmp(entry->key.data, key->data, entry->key.size) == 0
           )
       {
           return entry->index;
       }
       entry = entry->next;
   }
   return -1; // Column not found
}

// End HashTable

static void r_trim(String_View *str)
{
    while (str->size > 0 && isspace(str->data[str->size - 1]))
    {
        str->size--;
    }
}

static void l_trim(String_View *str)
{
    while (str->size > 0 && isspace(str->data[0]))
    {
        str->data++;
        str->size--;
    }
}

static void trim(String_View *str)
{
    l_trim(str);
    r_trim(str);
}


void init_csv(CSV *csv)
{
    csv->cols_count = 0;
    csv->rows_count = 0;
    csv->rows = NULL;
    csv->header = NULL;
    csv->type = NULL;
}

void deinit_csv(CSV *csv)
{
    arena_free(&tempArena);
}

static u64 count_rows_from_buffer(u8 *buffer)
{
    u64 rows = 0;
    u8 *ptr = buffer;
    while ((ptr = strchr(ptr, '\n')))
    {
        rows++;
        ptr++;
    }
    return rows + 1;
}

static u64 count_columns_from_buffer(u8 *buffer)
{
    u64 cols = 0;
    u8 *ptr = buffer;
    while (*ptr && (*ptr == '\n' || *ptr == '\r'))
    {
        ptr++;
    }

    while (*ptr && *ptr != '\n')
    {
        if (*ptr == ';' || *ptr == ',')
        {
            cols++;
        }
        ptr++;
    }
    return cols + 1;
}

static ERRNO parse_header(CSV *csv, u8 *buffer)
{
    csv->header = (String_View *)arena_alloc(&tempArena, sizeof(String_View) * csv->cols_count);
    if (!csv->header)
    {
        return 0;
    }
    for (size_t i = 0; i < csv->cols_count; i++)
    {
        csv->header[i].data = NULL;
    }

    u8 *current = buffer;
    u64 col = 0;
    while (*current && col < csv->cols_count)
    {
        csv->header[col].data = current;
        u8 *start = current;
        while (*current && *current != ';' && *current != ',' && *current != '\n')
        {
            current++;
        }
        csv->header[col].size = current - start;
        trim(&csv->header[col]);
        insert_into_hash(&csv->header[col], col);
        if (*current == ';' || *current == ',')
        {
            current++;
        } 
        col++;
    }
    return 1;
}


static ERRNO parse(CSV *csv, u8 *buffer)
{
    csv->rows = (Row *)arena_alloc(&tempArena, sizeof(Row) * (csv->rows_count - 1));
    if (!csv->rows)
    {
        return 0;
    }
    
    u8 *current = buffer;
    for (s64 row = 0; row < csv->rows_count - 1; row++)
    {   
        csv->rows[row].cells = (String_View *)arena_alloc(&tempArena, sizeof(String_View) * csv->cols_count);
        if (!csv->rows[row].cells)
        {
            return 0;
        }
        for (size_t i = 0; i < csv->cols_count; i++)
        {
            csv->rows[row].cells[i].data = NULL;
        }

        u64 col = 0;
        while (*current && col < csv->cols_count)
        {
            csv->rows[row].cells[col].data = current;
            u8 *start = current;
            while (*current && *current != ';' && *current != ',' && *current != '\n')
            {
                current++;
            }
            csv->rows[row].cells[col].size = current - start;
            trim(&csv->rows[row].cells[col]);   
            if (*current == ';' || *current == ',')
            {
                current++;
            }
            col++;
        }

        if (*current == '\0')
        {
            break;
        }

        if (*current == '\n')
        {
            current++;
        }
    }
    return 1;
}



static boolean is_bool(String_View data)
{
    return (data.size == 4 && (!strncmp(data.data, "TRUE", 4) || !strncmp(data.data, "true", 4))) ||
           (data.size == 5 && (!strncmp(data.data, "FALSE", 5) || !strncmp(data.data, "false", 5)));
}

static void detect_column_type(CSV *csv, u32 col)
{
    boolean is_integer = TRUE, is_float = FALSE, is_boolean = TRUE;

    for (size_t row = 0; row < csv->rows_count - 1; row++)
    {
        String_View data = csv->rows[row].cells[col];
        if (data.size == 0 || data.data == NULL)
        {
            continue;
        }

        if (!is_bool(data))
        {
            is_boolean = FALSE;
        }

        boolean has_dot = FALSE;
        boolean neg = FALSE;
        for (size_t c = 0; c < data.size; c++)
        {
            char ch = data.data[c];

            if (!isdigit(ch))
            {
                if (ch == '-' && c + 1 < data.size && isdigit(data.data[c + 1]))
                {
                    neg = TRUE;
                }
                else if (ch == '.' && !has_dot && c > 0)
                {
                    is_float = TRUE;
                    has_dot = TRUE;
                    is_integer = FALSE;
                }
                else
                {
                    is_integer = FALSE;
                    is_float = FALSE;
                    break;
                }
            }
        }
    }

    if (is_integer)
    {
        csv->type[col] = CSV_TYPE_INTEGER;
    }
    else if (is_boolean)
    {
        csv->type[col] = CSV_TYPE_BOOLEAN;
    }
    else if (is_float)
    {
        csv->type[col] = CSV_TYPE_FLOAT;
    }
    else
    {
        csv->type[col] = CSV_TYPE_STRING;
    }
}


ERRNO read_csv(const char *content, CSV *csv)
{
    FILE *file = fopen(content, "rb");
    if (!file)
    {
        goto defer;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    if (file_size < 0)
    {
        goto defer;
    }
    rewind(file);

    u8 *buffer = arena_alloc(&tempArena, file_size + 1);
    if (!buffer)
    {
        goto defer;
    }
    

    if ((fread(buffer, 1, file_size, file)) < 0)
    {
        goto defer;
    }
    buffer[file_size] = '\0';

    csv->rows_count = count_rows_from_buffer(buffer);
    csv->cols_count = count_columns_from_buffer(buffer);

    if (!parse_header(csv, buffer))
    {
        goto defer;
    }

    while (*buffer && *buffer != '\n')
    {
        buffer++;
    }
    buffer++;
    
    if (!parse(csv, buffer))
    {
        goto defer;
    }


    csv->type = (ColumnType *)arena_alloc(&tempArena, sizeof(ColumnType) * csv->cols_count);
    if (!csv->type)
    {
        goto defer;
    }

    for (size_t col = 0; col < csv->cols_count; col++)
    {
        detect_column_type(csv, col);
    }
    
    fclose(file);
    return 1;

defer:
    if (file)
    {
        fclose(file);
    }

    if (buffer)
    {
        arena_free(&tempArena);
    }
    return 0;
}

ERRNO save_csv(const char *output_file, CSV *csv)
{
    if (!csv)
    {
        return 0;
    }

    const char *path_to_file = output_file ? output_file : "out.csv";
    FILE *target = fopen(path_to_file, "wb");
    if (!target)
    {
        return 0;
    }

    char buffer[BUFFER_SIZE];
    size_t buf_len = 0;
    for (s64 col = 0; col < get_col_count(csv); col++)
    {
        size_t len = csv->header[col].size;
        if (buf_len + len + 2 >= BUFFER_SIZE)
        {
            fwrite(buffer, 1, buf_len, target);
            buf_len = 0;
        }

        memcpy(buffer + buf_len, csv->header[col].data, len);
        buf_len += len;
        buffer[buf_len++] = (col == get_col_count(csv) - 1) ? '\n' : ',';
    }

    fwrite(buffer, 1, buf_len, target);
    buf_len = 0;
    for (s64 row = 0; row < get_row_count(csv) - 1; row++)
    {
        for (s64 col = 0; col < get_col_count(csv); col++)
        {
            size_t len = csv->rows[row].cells[col].size;
            if (buf_len + len + 2 >= BUFFER_SIZE)
            {
                fwrite(buffer, 1, buf_len, target);
                buf_len = 0;
            }

            memcpy(buffer + buf_len, csv->rows[row].cells[col].data, len);
            buf_len += len;
            buffer[buf_len++] = (col == get_col_count(csv) - 1) ? '\n' : ',';
        }
    }

    if (buf_len > 0)
    {
        fwrite(buffer, 1, buf_len, target);
    }

    fclose(target);
    return 1;
}


void print_csv(CSV *csv)
{
    for (size_t col = 0; col < csv->cols_count; col++)
    {
        printf("%-12.*s", (int)csv->header[col].size, csv->header[col].data);
        printf(" Type: ");
        
        switch (csv->type[col])
        {
            case CSV_TYPE_INTEGER:
                printf("Integer");
                break;
            case CSV_TYPE_FLOAT:
                printf("Float");
                break;
            case CSV_TYPE_BOOLEAN:
                printf("Boolean");
                break;
            case CSV_TYPE_STRING:
                printf("String");
                break;
            default:
                printf("UNKNOWN");
                break;
        }
        printf(" | ");
    }
    printf("\n");
    printf("------------------------------------\n");
    for (size_t row = 0; row < csv->rows_count - 1; row++)
    {
        for (size_t col = 0; col < csv->cols_count; col++)
        {
            printf("%-24.*s", (int)csv->rows[row].cells[col].size, csv->rows[row].cells[col].data);
        }
        printf("\n");
    }
}

void fillna(CSV *csv)
{
    for (s64 row = 0; row < csv->rows_count - 1; row++)
    {
        for (s64 col = 0; col < csv->cols_count; col++)
        {
            if (csv->rows[row].cells[col].size == 0 || csv->rows[row].cells[col].data == NULL)
            {
                if (csv->type[col] == CSV_TYPE_FLOAT || csv->type[col] ==  CSV_TYPE_INTEGER)
                {
                    csv->rows[row].cells[col].data = "NaN";
                    csv->rows[row].cells[col].size = 3;
                } 
                else 
                {
                    csv->rows[row].cells[col].data = "None";
                    csv->rows[row].cells[col].size = 4;
                }
            }
        }
    }
}

s64 to_integer(String_View cell)
{
    if (cell.size == 0 || cell.data == NULL)
    {
        return -1;
    }
    s8 buffer[21];
    s8 *endptr;
    memcpy(buffer, cell.data, cell.size);
    buffer[cell.size] = '\0';
    return strtoll(buffer, &endptr, 10);
}

double to_float(String_View cell)
{
    if (cell.size == 0 || cell.data == NULL)
    {
        return -1.0;
    }
    s8 buffer[21];
    s8 *endptr;
    memcpy(buffer, cell.data, cell.size);
    buffer[cell.size] = '\0';
    return strtold(buffer, &endptr);
}

boolean is_cell_empty(String_View cell)
{
    if (cell.size == 0 || cell.data == NULL)
    {
        return TRUE;
    }
    return FALSE;
}

ERRNO convert_cell_to_integer(CSV *csv, u32 row, u32 col, s64 *output)
{
    if (!csv || !output)
    {
        return 0;
    }

    if (row >= csv->rows_count || col >= csv->cols_count)
    {
        return 0;
    }

    if (csv->type[col] != CSV_TYPE_INTEGER)
    {
        return 0;
    }

    String_View cell = csv->rows[row - 1].cells[col];
    if (cell.size == 0)
    {
        return 0;
    }

    s8 buffer[21];
    memcpy(buffer, cell.data, cell.size);
    buffer[cell.size] = '\0';

    s8 *endptr;
    *output = strtoll(buffer, &endptr, 10);
    return 1;
}

ERRNO convert_cell_to_float(CSV *csv, u32 row, u32 col, double *output)
{
    if (!csv || !output)
    {
        return 0;
    }

    if (row >= csv->rows_count || col >= csv->cols_count)
    {
        return 0;
    }

    if (csv->type[col] != CSV_TYPE_FLOAT)
    {
        return 0;
    }

    String_View cell = csv->rows[row - 1].cells[col];
    if (cell.size == 0)
    {
        return 0;
    }

    s8 buffer[21];
    memcpy(buffer, cell.data, cell.size);
    buffer[cell.size] = '\0';

    s8 *endptr;
    *output = strtold(buffer, &endptr);
    return 1;
}

u64 get_row_count(CSV *csv)
{
    return csv->rows_count;
}

u64 get_col_count(CSV *csv)
{
    return csv->cols_count;
}

const String_View *get_header(CSV *csv)
{
    if (!csv)
    {
        return NULL;
    }
    return csv->header;
}

String_View get_cell(CSV *csv, u32 row, String_View *column_name)
{
    s64 col = get_column_index(column_name);
    if (col == -1)
    {
        return sv_null;
    }
    return csv->rows[row - 1].cells[col];
}

const String_View *get_row_at(CSV *csv, u32 idx)
{
    if (idx >= csv->rows_count)
    {
        return NULL;
    }
    return csv->rows[idx].cells;
}

ERRNO append_column(CSV *csv, String_View *column_to_append, u32 rows)
{
    if (!csv || !column_to_append)
    {
        return 0;
    }

    if (rows != csv->rows_count)
    {
        return 0;
    }
    u32 new_col_index = csv->cols_count;
    csv->cols_count++;
    csv->header = arena_realloc(
                                    &tempArena,
                                    csv->header,
                                    (csv->cols_count - 1) * sizeof(String_View),
                                    csv->cols_count * sizeof(String_View)
                               );       
    if (!csv->header)
    {
        return 0;
    }
    csv->header[new_col_index] = column_to_append[0];

    for (u32 row = 0; row < rows - 1; row++)
    {
        csv->rows[row].cells = arena_realloc(
                                                &tempArena, 
                                                csv->rows[row].cells, 
                                                (csv->cols_count - 1) * sizeof(String_View), 
                                                csv->cols_count * sizeof(String_View)
                                            );
        if (!csv->rows[row].cells)
        {
            return 0;
        }
        csv->rows[row].cells[new_col_index] = column_to_append[row + 1];
    }
    detect_column_type(csv, new_col_index);
    return 1;
}

ERRNO append_many_columns(CSV *csv, String_View **columns_to_append, u32 rows_to_append, u32 cols_to_append)
{
    if (!csv || !columns_to_append)
    {
        return 0;
    }

    if (rows_to_append != csv->rows_count)
    {
        return 0;
    }

    for (u32 col = 0; col < cols_to_append; col++)
    {
        if (!append_column(csv, columns_to_append[col], rows_to_append))
        {
            return 0;
        }
    }

    return 1;
}

ERRNO append_row(CSV *csv, String_View *row_to_append, u32 cols_to_append)
{
    if (!csv || !row_to_append)
    {
        return 0;
    }

    if (cols_to_append != csv->cols_count)
    {
        return 0;
    }

    // This is mess hahaha
    u64 rows, new_rows;
    rows = csv->rows_count - 1;
    new_rows = rows + 1;
    u64 new_row_index = rows;
    csv->rows_count++;
    csv->rows = (Row *)arena_realloc(&tempArena, csv->rows, sizeof(Row *) * (rows), sizeof(Row *) * (new_rows - 1));
    if (!csv->rows)
    {
        return 0;
    }
    csv->rows[new_row_index].cells = row_to_append;
    // Needs check if each cell from new row matches the column type
    return 1;
}

ERRNO append_many_rows(CSV *csv, String_View **rows_to_append, u32 many_rows, u32 many_cols)
{
    if (!csv || !rows_to_append)
    {
        return 0;
    }

    if (many_cols != get_col_count(csv))
    {
        return 0;
    }


    for (s64 row = 0; row < many_rows; row++)
    {
        if (!append_row(csv, rows_to_append[row], many_cols))
        {
            return 0;
        }
    }
    return 1;
}


String_View *csv_filter(CSV *csv, String_View column_name, boolean (*predicate)(String_View cell), u64 *out_count)
{
    if (!csv || !predicate || !out_count)
    {
        return NULL;
    }

    s64 col = get_column_index(&column_name);
    if (col == -1)
    {
        return NULL;
    }

    *out_count = 0;
    for (u32 row = 0; row < csv->rows_count - 1; row++)
    {
        if (predicate(csv->rows[row].cells[col]))
        {
            (*out_count)++;
        }
    }

    if (*out_count == 0)
    {
        return NULL;
    }

    String_View *filtered_cells = arena_alloc(&tempArena, sizeof(String_View) * (*out_count));
    if (!filtered_cells)
    {
        return NULL;
    }

    u32 index = 0;
    for (u32 row = 0; row < csv->rows_count - 1; row++)
    {
        if (predicate(csv->rows[row].cells[col]))
        {
            filtered_cells[index++] = csv->rows[row].cells[col];
        }
    }
    return filtered_cells;
}


ERRNO csv_mean(CSV *csv, String_View column_name, double *output)
{
    if (!csv || column_name.size == 0 || !output)
    {
        return 0;
    }

    s64 col = get_column_index(&column_name);
    if (col == -1)
    {
        return 0;
    }

    if (csv->type[col] != CSV_TYPE_INTEGER && csv->type[col] != CSV_TYPE_FLOAT)
    {
        return 0;
    }

    double sum = 0.0;
    for (s64 row = 0; row < get_row_count(csv) - 1; row++)
    {
        if (!is_cell_empty(csv->rows[row].cells[col]))
        {
            sum += to_float(csv->rows[row].cells[col]);
        }
    }
    if (get_row_count(csv) == 0)
    {
        return 0;
    }
    *output = sum / (get_row_count(csv) - 1);
    return 1;
}

static s32 cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

ERRNO csv_median(CSV *csv, String_View column_name, double *output)
{
    if (!csv || column_name.size == 0 || !output)
    {
        return 0;
    }

    s64 col = get_column_index(&column_name);
    if (col == -1 || (csv->type[col] != CSV_TYPE_INTEGER && csv->type[col] != CSV_TYPE_FLOAT))
    {
        return 0;
    }

    u64 row_count = get_row_count(csv) - 1;
    if (row_count == 0)
    {
        return 0;
    }

    double *values = (double *)malloc(row_count * sizeof(double));
    if (!values)
    {
        return 0;
    }

    u64 valid_count = 0;
    for (u64 row = 0; row < row_count; row++)
    {
        if (!is_cell_empty(csv->rows[row].cells[col]))
        {
            values[valid_count++] = to_float(csv->rows[row].cells[col]);
        }
    }

    if (valid_count == 0)
    {
        return 0;
    }

    qsort(values, valid_count, sizeof(double), (int (*)(const void *, const void *))cmp_double);

    if (valid_count % 2 == 1)
    {
        *output = values[valid_count / 2];
    }
    else
    {
        *output = (values[valid_count / 2 - 1] + values[valid_count / 2]) / 2.0;
    }
    free(values);
    return 1;
}

ERRNO csv_sd(CSV *csv, String_View column_name, double *output)
{
    if (!csv || column_name.size == 0 || !output)
    {
        return 0;
    }

    s64 col = get_column_index(&column_name);
    if (col == -1 || (csv->type[col] != CSV_TYPE_INTEGER && csv->type[col] != CSV_TYPE_FLOAT))
    {
        return 0;
    }

    u64 row_count = get_row_count(csv) - 1;
    if (row_count == 0)
    {
        return 0;
    }

    double sum = 0.0, sum_sq = 0.0;
    u64 valid_count = 0;

    for (u64 row = 0; row < row_count; row++)
    {
        if (!is_cell_empty(csv->rows[row].cells[col]))
        {
            double value = to_float(csv->rows[row].cells[col]);
            sum += value;
            sum_sq += value * value;
            valid_count++;
        }
    }

    if (valid_count < 2)
    {
        return 0;
    }
    double mean = sum / valid_count;
    *output = sqrt((sum_sq / valid_count) - (mean * mean));
    return 1;
}