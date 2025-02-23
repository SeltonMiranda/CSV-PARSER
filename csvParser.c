#include "csvParser.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ALIGNMENT 16  
#define ALIGN_UP(x, a) (((x) + (a - 1)) & ~(a - 1))

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

static u64 hash_function(const char *str) {
    u64 hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % BUCKETS;
}


void insert_into_hash(HashTable *table, u8 *key, s32 index, Arena *a)
{
   u64 h = hash_function(key);
   HashEntry *new_entry = arena_alloc(a, sizeof(HashEntry));
   new_entry->key = arena_alloc(a, strlen(key) + 1);
   strcpy(new_entry->key, key);
   new_entry->index = index;
   new_entry->next = table->buckets[h];
   table->buckets[h] = new_entry;
}

s32 get_column_index(CSV *csv, const char *key)
{
   u64 h = hash_function(key);
   HashEntry *entry = csv->index_table.buckets[h];
   while (entry) 
   {
       if (strcmp(entry->key, key) == 0)
       {
           return entry->index;
       }
       entry = entry->next;
   }
   return -1; // Column not found
}

static Arena tempArena = {0};
static Arena tempColArrayArena = {0};

void init_csv(CSV *csv)
{
    csv->cols_count = 0;
    csv->rows_count = 0;
    csv->header = NULL;
    csv->rows = NULL;
    csv->type = NULL;
    for (size_t i = 0; i < BUCKETS; i++)
    {
        csv->index_table.buckets[i] = NULL;
    }
}

void deinit_csv(CSV *csv)
{
    arena_free(&tempArena);
}

static void r_trim(char *str) {
    char *end = str + strlen(str) - 1;  
    while (end >= str && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0'; 
}


static void l_trim(char *str) {
    size_t start = 0;
    while (isspace((unsigned char)str[start])) {
        start++;
    }

    if (start > 0) {
        memmove(str, str + start, strlen(str) - start + 1);
    }
}

static void trim(char *str)
{
    l_trim(str);
    r_trim(str);
}

static u8 is_bool(u8 *data)
{
    return  strcmp(data, "true")  == 0 ||
            strcmp(data, "TRUE")  == 0 ||
            strcmp(data, "false") == 0 ||
            strcmp(data, "FALSE") == 0;
}

static void detect_column_type(CSV *csv, u32 col)
{
    u8 is_integer, is_float, is_boolean, is_string;
    is_boolean = 1;
    is_integer = 1;
    is_float = 0;
    
    for (size_t row = 0; row < csv->rows_count; row++)
    {
        u8 *data = csv->rows[row].data[col];
        if (!data || !(*data)) // if data is empty, skip
        {
            continue;
        }

        if (!is_bool(data))
        {
            is_boolean = 0;
        }

        u8 has_dot = 0;
        for (size_t c = 0; data[c]; c++)
        {
            if (!isdigit(data[c]))
            {
                // DO LATER -> check wheter "dot" is the first character, if so,
                // append "0" before it, making it a float type (or may do it in the convert_cell_to_float function)
                if ((data[c] == '.' && !has_dot) && c > 0)
                {
                    is_float = 1;
                    has_dot = 1;
                    is_integer = 0;
                }
                else
                {
                    is_integer = 0;
                    is_float = 0;
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

static u32 count_fields(const char* header, u8 delimiter)
{
    u32 fields = 0;
    const u8 *ptr = header;
    while ((ptr = strchr(ptr, delimiter)) != NULL)
    {
        fields++;
        ptr++;
    }
    return fields + 1;
}

static u32 count_rows(FILE *file)
{
    char line[1024];
    u32 num_lines = 0;
    u32 row = 0;
    while ((fgets(line, sizeof(line), file)))
    {
        num_lines++;
    }
    rewind(file);
    fgets(line, sizeof(line), file); // Retrieve header
    return num_lines; 
}

static s32 parse_header(CSV *csv, FILE *file)
{

    u8 header[1024];
    if (!fgets(header, sizeof(header), file))
    {
        return 0;
    }

    csv->cols_count = count_fields(header, ',');
    csv->type = (ColumnType *)arena_alloc(&tempArena, sizeof(ColumnType) * csv->cols_count);
    csv->header = (u8**)arena_alloc(&tempArena, sizeof(u8 *) * csv->cols_count);
    if (!csv->header || !csv->type)
    {
        return 0;
    }

    u8 *header_token = strtok((char *)header, ";,\n");
    u32 i = 0;
    while (header_token)
    {
        trim(header_token);
        csv->header[i] = arena_alloc(&tempArena, strlen(header_token) + 1);
        if (!csv->header[i])
        {
            return 0;
        }
        insert_into_hash(&csv->index_table, header_token, i, &tempArena);
        strcpy(csv->header[i], header_token);
        header_token = strtok(NULL, ";,\n");
        i++;
    }

    return 1;
}

static u8 *get_token_before_delim(const u8 *str, const u8 *delims, const u8**next) {
    u8 *first_delim = NULL;
    u8 *NULL_TOKEN = "NULL";
    for (size_t i = 0; delims[i] != '\0'; i++)
    {
        u8 *pos = strchr(str, delims[i]);
        if (pos && (!first_delim || pos < first_delim))
        {
            first_delim = pos;
        }
    }

    (*next) = first_delim ? first_delim + 1 : NULL;
    size_t len = first_delim ? (size_t)(first_delim - str) : strlen(str);

    if (len == 0)
    {
        char *null_str = arena_alloc(&tempArena, strlen(NULL_TOKEN) + 1);
        if (!null_str)
        {
            return NULL;
        }    
        strcpy(null_str, NULL_TOKEN);
        return null_str;
    }

    u8 *token = arena_alloc(&tempArena, len + 1);
    if (!token)
    {
        return NULL;
    } 

    strncpy(token, str, len);
    token[len] = '\0';

    return token;
}

static s32 parse(CSV *csv, FILE *file)
{
    csv->rows_count = count_rows(file);
    csv->rows = arena_alloc(&tempArena, sizeof(Row) * csv->rows_count);
    if (!csv->rows)
    {
        return 0;
    }

    u8 line[1024];
    u32 row = 0;
    while (fgets(line, sizeof(line), file))
    {
        csv->rows[row].data = arena_alloc(&tempArena, sizeof(u8 *) * csv->cols_count);
        if (!csv->rows[row].data)
        {
            return 0;
        }

        const u8*current = line;
        const u8 *next = NULL;
        u32 col = 0;
        while (current && *current && col < csv->cols_count)
        {
            u8 *token = get_token_before_delim(current, ";,\n", &next);
            if (!token)
            {
                return 0;
            }

            csv->rows[row].data[col] = arena_alloc(&tempArena, strlen(token) + 1);
            if (!csv->rows[row].data[col])
            {
                free(token);
                return 0;
            }
            strcpy(csv->rows[row].data[col], token);
            current = next;
            col++;
        }
        row++;
    }

    for (size_t col = 0; col < csv->cols_count; col++)
    {
        detect_column_type(csv, col);
    }
    return 1;
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


    if (!parse_header(csv, file))
    {
        goto defer;
    }

    if (!parse(csv, file))
    {
        goto defer;
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
        arena_free(&tempArena)
    }
    return 0;
}

u64 get_row_count(CSV *csv)
{
    return csv == NULL ? 0 : (csv->rows_count + 1);
}

u64 get_col_count(CSV *csv)
{
    return csv == NULL ? 0 : csv->cols_count;
}

u8 **get_header(CSV *csv)
{
    return csv == NULL ? NULL : csv->header;
}

u8 **get_row_at(CSV *csv, u32 idx)
{
    return (csv == NULL || idx < 0) ? NULL : csv->rows[idx].data;
}

void print_csv(CSV *csv)
{
    for (size_t col = 0; col < csv->cols_count; col++)
    {
        printf("%12s", csv->header[col]);
        printf(" Type:");
        switch (csv->type[col])
        {
            case CSV_TYPE_INTEGER:
            {
                printf("Integer");
            }
            break;

            case CSV_TYPE_FLOAT:
            {
                printf("Float");
            }
            break;

            case CSV_TYPE_BOOLEAN:
            {
                printf("Boolean");
            }
            break;

            case CSV_TYPE_STRING:
            {
                printf("String");
            }
            break;

            default:
            {
                printf("UNKNOWN");
            }
            break;
        }
    }
    printf("\n");
    printf("------------------------------------\n");
    for (size_t row = 0; row < csv->rows_count; row++)
    {
        for (size_t col = 0; col < csv->cols_count; col++)
        {
            printf("%24s", csv->rows[row].data[col]);
        }
        printf("\n");
    }
}

u8 **get_column_at(CSV *csv, const u8 *column_name)
{
    s32 idx = get_column_index(csv, column_name);
    if (idx == -1)
    {
        return NULL;
    }

    u8 **buffer = (u8**)arena_alloc(&tempArena, sizeof(u8 *) * csv->rows_count);
    if (!buffer)
    {
        return NULL;
    }

    for (s32 row = 0; row < csv->rows_count; row++)
    {
        buffer[row] = arena_alloc(&tempArena, strlen(csv->rows[row].data[idx]) + 1);
        if (!buffer[row])
        {
            return NULL;
        }
        strcpy(buffer[row], csv->rows[row].data[idx]);
    }

    return buffer;
}

ERRNO convert_cell_to_integer(CSV *csv, u32 at_row, u32 at_col, s64 *output)
{
    if (
            !csv || at_row >= get_row_count(csv) ||
            at_row < 0 || !output || csv->type[at_col] != CSV_TYPE_INTEGER
       )
    {
        return -1;
    }

    u8 *data = (u8 *)csv->rows[at_row].data[at_col];
    if (!data || !(*data) || strcmp(data, "NULL") == 0)
    {
        *output = 0;
        return 1;
    }

    s8 *endptr;
    s64 value = strtoll(data, &endptr, 10);
    if (*endptr != '\0')
    {
        return -1;
    }

    *output = value;
    return 1;
}

ERRNO convert_cell_to_float(CSV *csv, u32 at_row, u32 at_col, double *output)
{


    if (
        !csv || at_row >= get_row_count(csv) ||
        at_row < 0 || !output || csv->type[at_col] != CSV_TYPE_FLOAT
       )
    {
        return -1;
    }

    u8 *data = (u8 *)csv->rows[at_row].data[at_col];
    if (!data || !(*data) || strcmp(data, "NULL") == 0)
    {
        *output = 0.0;
        return 1;
    }

    s8 *endptr;
    s64 value = strtold(data, &endptr);
    if (*endptr != '\0')
    {
        return -1;
    }

    *output = value;
    return 1;
}

ERRNO convert_column_to_integer(CSV *csv, u32 col, s64 col_output[])
{
    if (csv->type[col] != CSV_TYPE_INTEGER)
    {
        return 0;
    }

    for (size_t row = 0; row < get_row_count(csv) - 1; row++)
    {
        if (!convert_cell_to_integer(csv, row, col, &col_output[row]))
        {
            return 0;
        }
    }
    return 1;
}

ERRNO convert_column_to_float(CSV *csv, u32 col, double col_output[])
{
    if (csv->type[col] != CSV_TYPE_FLOAT)
    {
        return 0;
    }

    for (size_t row = 0; row < get_row_count(csv) - 1; row++)
    {
        if (!convert_cell_to_float(csv, row, col, &col_output[row]))
        {
            return 0;
        }
    }
    return 1;
}

static ERRNO append_new_field(CSV *csv, u8 *new_field)
{
    u32 old_cols = get_col_count(csv);
    u32 new_cols = old_cols + 1;
    csv->type = arena_realloc(
                                &tempArena,
                                csv->type,
                                sizeof(ColumnType) * old_cols,
                                sizeof(ColumnType) * new_cols
                             );
    if (!csv->type)
    {
        return 0;
    }

    csv->header = arena_realloc(
                                    &tempArena,
                                    csv->header,
                                    sizeof(u8 *) * old_cols,
                                    sizeof(u8 *) * new_cols
                               );
    csv->header[new_cols - 1] = arena_alloc(&tempArena, strlen(new_field) + 1);
    if (!csv->header[new_cols - 1])
    {
        return 0;
    }
    strcpy(csv->header[new_cols - 1], new_field);
    return 1;
}

ERRNO append_column(CSV *csv, u8 **column_to_append, u32 rows)
{
    if (!csv || !column_to_append || rows != get_row_count(csv)) // diff types of errors, gotta split them later
    {
        return 0;
    }
    if (!append_new_field(csv, *column_to_append))
    {
        
        return 0;
    }

    u32 old_cols = get_col_count(csv);
    u32 new_cols = old_cols + 1;
    for (s64 row = 0; row < get_row_count(csv) - 1; row++)
    {
        csv->rows[row].data = arena_realloc(
                                            &tempArena,
                                            csv->rows[row].data,
                                            sizeof(u8 *) * old_cols,
                                            sizeof(u8 *) * new_cols
                                           );
        if (!csv->rows[row].data)
        {
            return 0;
        }

        if (column_to_append[row + 1])
        {
            csv->rows[row].data[new_cols - 1] = arena_alloc(&tempArena, strlen(column_to_append[row + 1]) + 1);
            if (!csv->rows[row].data[new_cols - 1])
            {
                return 0;
            }
            strcpy(csv->rows[row].data[new_cols - 1], column_to_append[row + 1]);
        }
        else
        {
            csv->rows[row].data[new_cols - 1] = arena_alloc(&tempArena, strlen("NULL") + 1);
            strcpy(csv->rows[row].data[new_cols - 1], "NULL");
        }
    }
    detect_column_type(csv, new_cols - 1);
    csv->cols_count++;
    return 1;
}

ERRNO append_many_columns(CSV *csv, u8 ***columns_to_append, u32 rows, u32 cols)
{
    if (!columns_to_append)
    {
        return 0;
    }

    for (s64 col = 0; col < cols; col++)
    {
        if (!append_column(csv, columns_to_append[col], rows))
        {
            return 0;
        }
    }
    return 1;
}