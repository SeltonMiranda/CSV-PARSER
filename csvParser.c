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


void insert_into_hash(HashTable *table, char *key, size_t index, Arena *a)
{
   u64 h = hash_function(key);
   HashEntry *new_entry = arena_alloc(a, sizeof(HashEntry));
   new_entry->key = arena_alloc(a, strlen(key) + 1);
   strcpy(new_entry->key, key);
   new_entry->index = index;
   new_entry->next = table->buckets[h];
   table->buckets[h] = new_entry;
}

u32 get_column_index(HashTable *table, const char *key)
{
   u64 h = hash_function(key);
   HashEntry *entry = table->buckets[h];
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

void init_csv(CSV *csv)
{
    csv->cols_count = 0;
    csv->rows_count = 0;
    csv->headers = NULL;
    csv->rows = NULL;
    csv->allocator.begin = NULL;
    csv->allocator.end = NULL;
}

void deinit_csv(CSV *csv)
{
    arena_free(&csv->allocator);
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


static u32 count_fields(const char* header, u8 delimiter)
{
    u32 fields = 0;
    const char *ptr = header;
    while ((ptr = strchr(ptr, delimiter)) != NULL)
    {
        fields++;
        ptr++;
    }
    return fields + 1;
}

ERRNO read_csv(const char *content, CSV *csv)
{
    FILE *file = fopen(content, "rb");
    if (!file)
    {
        goto defer;
    }

    u8 headers[1024];
    if (!fgets(headers, sizeof(headers), file))
    {
        goto defer;
    }

    csv->cols_count = count_fields(headers, ',');
    csv->headers = (u8**)arena_alloc(&csv->allocator, sizeof(u8 *) * csv->cols_count);
    if (!csv->headers)
    {
        return 0;
    }

    u8 *header_token = strtok((char *)headers, ",\n");
    u32 i = 0;
    while (header_token)
    {
        trim(header_token);
        csv->headers[i] = arena_alloc(&csv->allocator, strlen(header_token) + 1);
        if (!csv->headers[i])
        {
            return 0;
        }
        strcpy(csv->headers[i], header_token);
        header_token = strtok(NULL, ",\n");
        i++;
    }

    char line[1024];
    u32 num_lines = 0;
    while ((fgets(line, sizeof(line), file)))
    {
        ++num_lines;
    }
    csv->rows_count = num_lines - 1;
    rewind(file);
    fgets(line, sizeof(line), file);


    csv->rows = arena_alloc(&csv->allocator, sizeof(Row) * num_lines);
    if (!csv->rows)
    {
        return 0;
    }

    u32 row = 0;
    while (fgets(line, sizeof(line), file))
    {
        csv->rows[row].data = arena_alloc(&csv->allocator, sizeof(u8 *) * csv->cols_count);
        if (!csv->rows[row].data)
        {   
            return 0;
        }

        char *token = strtok(line, ",\n");
        u32 col = 0;
        while (token)
        {
            csv->rows[row].data[col] = arena_alloc(&csv->allocator, strlen(token) + 1); 
            if (!csv->rows[row].data[col])
            {
                return 0;
            }
            strcpy(csv->rows[row].data[col], token);
            token = strtok(NULL, ",\n");
            col++;
        }
        row++;
    }
    fclose(file);

    return 1;

defer:
    if (file)
    {
        fclose(file);
    }

    return 0;
}