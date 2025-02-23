#include "csvParser.h"

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ALIGNMENT 16  
#define ALIGN_UP(x, a) (((x) + (a - 1)) & ~(a - 1))

static Arena tempArena = {0};


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

#include <ctype.h>
#include <string.h>
#include <stdbool.h>

static bool is_bool(String_View data)
{
    return (data.size == 4 && (!strncmp(data.data, "TRUE", 4) || !strncmp(data.data, "true", 4))) ||
           (data.size == 5 && (!strncmp(data.data, "FALSE", 5) || !strncmp(data.data, "false", 5)));
}

static void detect_column_type(CSV *csv, u32 col)
{
    bool is_integer = true, is_float = false, is_boolean = true;

    for (size_t row = 0; row < csv->rows_count - 1; row++)
    {
        String_View data = csv->rows[row].cells[col];
        if (data.size == 0 || data.data == NULL)
        {
            continue;
        }

        if (!is_bool(data))
        {
            is_boolean = false;
        }

        bool has_dot = false;
        for (size_t c = 0; c < data.size; c++)
        {
            char ch = data.data[c];

            if (!isdigit((unsigned char)ch))
            {
                if (ch == '.' && !has_dot && c > 0)
                {
                    is_float = true;
                    has_dot = true;
                    is_integer = false;
                }
                else
                {
                    is_integer = false;
                    is_float = false;
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
                csv->rows[row].cells[col].data = "None";
                csv->rows[row].cells[col].size = 4;
            }
        }
    }
}