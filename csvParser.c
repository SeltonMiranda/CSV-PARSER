#include "csvParser.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static HashTable indexTable = {0};
static s32 globalError = NIL;

static void set_error(s32 error)
{
    globalError = error;
}

static ERRNO get_error()
{
    return globalError;
}

static void print_error() {
    switch (globalError) {
        case ERR_MEM_ALLOC:
            printf("Erro: Falha na alocação de memória.\n");
            break;
        case ERR_FILE_NOT_FOUND:
            printf("Erro: Arquivo não encontrado.\n");
            break;
        case ERR_OPEN_FILE:
            printf("Erro: Falha ao abrir o arquivo.\n");
            break;
        case ERR_CSV_EMPTY:
            printf("Erro: O arquivo CSV está vazio.\n");
            break;
        case ERR_EMPTY_CELL:
            printf("Erro: Célula vazia detectada onde não deveria estar.\n");
            break;
        case ERR_CSV_OUT_OF_BOUNDS:
            printf("Erro: Índice fora dos limites do CSV.\n");
            break;
        case ERR_CSV_DIFF_TYPE:
            printf("Erro: Tipo de dado incompatível.\n");
            break;
        case ERR_INVALID_COLUMN:
            printf("Erro: Nome de coluna inválido ou inexistente.\n");
            break;
        case ERR_INVALID_ARG:
            printf("Erro: Argumento inválido passado para a função.\n");
            break;
        case ERR_INCONSISTENT_COLUMNS:
            printf("Erro: Número inconsistente de colunas entre as linhas do CSV.\n");
            break;
        case ERR_UNKNOWN:
        default:
            printf("Erro desconhecido.\n");
            break;
    }
}

boolean error()
{
    if (get_error() != NIL)
    {
        print_error();
        return TRUE;
    }
    return FALSE;
}

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

static void insert_into_hash(CSV *csv, String_View *key, s32 index)
{
   u64 h = hash_function(key);
   HashEntry *new_entry = arena_alloc(&csv->allocator, sizeof(HashEntry));
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
    csv->allocator.begin = NULL;
    csv->allocator.end = NULL;
}

void deinit_csv(CSV *csv)
{
    arena_free(&csv->allocator);
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

static s32 parse_header(CSV *csv, u8 *buffer)
{
    csv->header = (String_View *)arena_alloc(&csv->allocator, sizeof(String_View) * csv->cols_count);
    if (!csv->header)
    {
        set_error(ERR_MEM_ALLOC);
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
        insert_into_hash(csv, &csv->header[col], col);
        if (*current == ';' || *current == ',')
        {
            current++;
        } 
        col++;
    }
    return 1;
}


static s32 parse(CSV *csv, u8 *buffer)
{
    csv->rows = (Row *)arena_alloc(&csv->allocator, sizeof(Row) * (csv->rows_count - 1));
    if (!csv->rows)
    {
        set_error(ERR_MEM_ALLOC);
        return 0;
    }
    
    u8 *current = buffer;
    for (s64 row = 0; row < csv->rows_count - 1; row++)
    {   
        csv->rows[row].cells = (String_View *)arena_alloc(&csv->allocator, sizeof(String_View) * csv->cols_count);
        if (!csv->rows[row].cells)
        {
            set_error(ERR_MEM_ALLOC);
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


void read_csv(const char *content, CSV *csv)
{
    FILE *file = fopen(content, "rb");
    if (!file)
    {
        set_error(ERR_FILE_NOT_FOUND);
        goto defer;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    u8 *buffer = arena_alloc(&csv->allocator, file_size + 1);
    if (!buffer)
    {
        set_error(ERR_MEM_ALLOC);
        goto defer;
    }
    

    fread(buffer, 1, file_size, file);
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


    csv->type = (ColumnType *)arena_alloc(&csv->allocator, sizeof(ColumnType) * csv->cols_count);
    if (!csv->type)
    {
        set_error(ERR_MEM_ALLOC);
        goto defer;
    }

    for (size_t col = 0; col < csv->cols_count; col++)
    {
        detect_column_type(csv, col);
    }
    
    fclose(file);
    return;

defer:
    if (file)
    {
        fclose(file);
    }

    if (buffer)
    {
        arena_free(&csv->allocator);
    }
    return;
}

void save_csv(const char *output_file, CSV *csv)
{
    if (!csv)
    {
        set_error(ERR_CSV_EMPTY);
        return;
    }

    const char *path_to_file = output_file ? output_file : "out.csv";
    FILE *target = fopen(path_to_file, "wb");
    if (!target)
    {
        set_error(ERR_OPEN_FILE);
        return;
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
}

void print_csv(CSV *csv)
{
    if (!csv)
    {
        set_error(ERR_INVALID_ARG);
        return;
    } 
    else if (is_csv_empty(csv))
    {
        set_error(ERR_CSV_EMPTY);
        return;
    }

    for (size_t col = 0; col < csv->cols_count; col++)
    {
        printf("%-15.*s", (int)csv->header[col].size, csv->header[col].data);
    }
    printf("\n");
    
    for (size_t col = 0; col < csv->cols_count; col++)
    {
        printf("-------------------");
    }
    printf("\n");
    

    for (size_t row = 0; row < csv->rows_count - 1; row++)
    {
        for (size_t col = 0; col < csv->cols_count; col++)
        {
            switch (csv->type[col])
            {
                case CSV_TYPE_INTEGER:
                    printf("%-15d", *((int*)csv->rows[row].cells[col].data));
                    break;
                case CSV_TYPE_FLOAT:
                    printf("%-15.4f", *((float*)csv->rows[row].cells[col].data));
                    break;
                case CSV_TYPE_BOOLEAN:
                    printf("%-15s", *((int*)csv->rows[row].cells[col].data) ? "True" : "False");
                    break;
                case CSV_TYPE_STRING:
                    printf("%-15.*s", (int)csv->rows[row].cells[col].size, csv->rows[row].cells[col].data);
                    break;
                default:
                    printf("%-15s", "UNKNOWN");
                    break;
            }
        }
        printf("\n");
    }
}

void print_column(const String_View *column, u64 rows)
{
    if (!column)
    {
        set_error(ERR_INVALID_ARG);
        return;
    }

    for (u64 row = 0; row < rows; row++)
    {
        printf(sv_fmt"\n", sv_args(column[row]));
    }
}

void fillna(CSV *csv)
{
    if (!csv || is_csv_empty(csv))
    {
        set_error(ERR_CSV_EMPTY);
        return;
    }

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

CSV dropna(CSV *input_csv)
{   
    if (!input_csv || is_csv_empty(input_csv))
    {
        set_error(ERR_CSV_EMPTY);
        return (CSV){0};
    }

    u64 valid_rows = 0;
    for (u64 row = 0; row < get_row_count(input_csv) - 1; row++)
    {
        boolean empty = FALSE;
        for (u64 col = 0; col < get_col_count(input_csv); col++)
        {
            if (is_cell_empty(input_csv->rows[row].cells[col]))
            {
                empty = TRUE;
                break;
            }
        }

        if (!empty)
        {
            valid_rows++;
        }
    }
    
    if (valid_rows == get_row_count(input_csv) - 1)
    {
        return (CSV){0};
    }

    CSV output_csv;
    init_csv(&output_csv);
    output_csv.cols_count = input_csv->cols_count;
    output_csv.rows_count = valid_rows;
    output_csv.type = input_csv->type;
    output_csv.header = arena_alloc(&output_csv.allocator, input_csv->cols_count * sizeof(String_View));
    if (!output_csv.header)
    {
        set_error(ERR_MEM_ALLOC);
        return (CSV){0};
    }
    memcpy(output_csv.header, input_csv->header, input_csv->cols_count * sizeof(String_View));
    output_csv.rows = arena_alloc(&output_csv.allocator, valid_rows * sizeof(Row));
    if (!output_csv.rows)
    {
        set_error(ERR_MEM_ALLOC);
        return (CSV){0};
    }

    u64 new_row = 0;
    for (u64 row = 0; row < get_row_count(input_csv) - 1; row++)
    {
        boolean empty = FALSE;
        for (u64 col = 0; col < get_col_count(input_csv); col++)
        {
            if (is_cell_empty(input_csv->rows[row].cells[col]))
            {
                empty = TRUE;
                break;
            }
        }

        if (!empty)
        {
            output_csv.rows[new_row++] = input_csv->rows[row];
        }
    }
    output_csv.rows_count++; // for header
    return output_csv;
}

s64 to_integer(String_View cell)
{
    if (cell.size == 0 || cell.data == NULL)
    {
        set_error(ERR_EMPTY_CELL);
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
        set_error(ERR_EMPTY_CELL);
        return -1.0;
    }
    s8 buffer[21];
    s8 *endptr;
    memcpy(buffer, cell.data, cell.size);
    buffer[cell.size] = '\0';
    return strtold(buffer, &endptr);
}

boolean is_csv_empty(CSV *csv)
{
    return csv->rows_count == 0 ? TRUE : FALSE;
}

boolean is_cell_empty(String_View cell)
{
    if (cell.size == 0 || cell.data == NULL || !strncmp(cell.data, "None", cell.size) || !strncmp(cell.data, "NaN", cell.size))
    {
        return TRUE;
    }
    return FALSE;
}

void convert_cell_to_integer(CSV *csv, u32 row, u32 col, s64 *output)
{
    if (!csv || !output)
    {
        set_error(ERR_CSV_EMPTY);
        return;
    }

    if (row >= csv->rows_count || col >= csv->cols_count)
    {
        set_error(ERR_CSV_OUT_OF_BOUNDS);
        return;
    }

    if (csv->type[col] != CSV_TYPE_INTEGER)
    {
        set_error(ERR_CSV_DIFF_TYPE);
        return;
    }

    String_View cell = csv->rows[row - 1].cells[col];
    if (cell.size == 0)
    {
        set_error(ERR_EMPTY_CELL);
        return;
    }

    s8 buffer[21];
    memcpy(buffer, cell.data, cell.size);
    buffer[cell.size] = '\0';

    s8 *endptr;
    *output = strtoll(buffer, &endptr, 10);
    return;
}

void convert_cell_to_float(CSV *csv, u32 row, u32 col, double *output)
{
    if (!csv || !output)
    {
        set_error(ERR_CSV_EMPTY);
        return;
    }

    if (row >= csv->rows_count || col >= csv->cols_count)
    {
        set_error(ERR_CSV_OUT_OF_BOUNDS);
        return;
    }

    if (csv->type[col] != CSV_TYPE_FLOAT)
    {
        set_error(ERR_CSV_DIFF_TYPE);
        return;
    }

    String_View cell = csv->rows[row - 1].cells[col];
    if (cell.size == 0)
    {
        set_error(ERR_EMPTY_CELL);
        return;
    }

    s8 buffer[21];
    memcpy(buffer, cell.data, cell.size);
    buffer[cell.size] = '\0';

    s8 *endptr;
    *output = strtold(buffer, &endptr);
    return;
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
        set_error(ERR_CSV_EMPTY);
        return NULL;
    }
    return csv->header;
}

String_View get_cell(CSV *csv, u32 row, String_View *column_name)
{
    s64 col = get_column_index(column_name);
    if (col == -1)
    {
        set_error(ERR_INVALID_COLUMN);
        return sv_null;
    }
    return csv->rows[row - 1].cells[col];
}

const String_View *get_row_at(CSV *csv, u32 idx)
{
    if (idx >= csv->rows_count)
    {
        set_error(ERR_INVALID_COLUMN);
        return NULL;
    }
    return csv->rows[idx].cells;
}

const String_View *get_column(CSV *csv, String_View column_name)
{
    if (!csv)
    {
        set_error(ERR_CSV_EMPTY);
        return NULL;
    }

    s32 column_index = get_column_index(&column_name);
    if (column_index == -1)
    {
        set_error(ERR_COLUMN_NOT_FOUND);
        return NULL;
    }

    String_View *ret = arena_alloc(&csv->allocator, sizeof(String_View) * get_row_count(csv));
    if (!ret)
    {
        set_error(ERR_MEM_ALLOC);
        return NULL;
    }

    ret[0] = csv->header[column_index];
    for (u64 row = 0; row < get_row_count(csv) - 1; row++)
    {
        ret[row + 1] = csv->rows[row].cells[column_index];
    }
    return ret;
}

void append_column(CSV *csv, String_View *column_to_append, u32 rows)
{
    if (!csv || !column_to_append)
    {
        set_error(ERR_INVALID_ARG);
        return;
    }

    if (rows != csv->rows_count)
    {
        set_error(ERR_CSV_OUT_OF_BOUNDS);
        return;
    }
    u32 new_col_index = csv->cols_count;
    csv->cols_count++;
    csv->header = arena_realloc(
                                    &csv->allocator,
                                    csv->header,
                                    (csv->cols_count - 1) * sizeof(String_View),
                                    csv->cols_count * sizeof(String_View)
                               );       
    if (!csv->header)
    {
        set_error(ERR_MEM_ALLOC);
        return;
    }
    csv->header[new_col_index] = column_to_append[0];

    for (u32 row = 0; row < rows - 1; row++)
    {
        csv->rows[row].cells = arena_realloc(
                                                &csv->allocator, 
                                                csv->rows[row].cells, 
                                                (csv->cols_count - 1) * sizeof(String_View), 
                                                csv->cols_count * sizeof(String_View)
                                            );
        if (!csv->rows[row].cells)
        {
            set_error(ERR_MEM_ALLOC);
            return;
        }
        csv->rows[row].cells[new_col_index] = column_to_append[row + 1];
    }
    detect_column_type(csv, new_col_index);
    return;
}

void append_many_columns(CSV *csv, String_View **columns_to_append, u32 rows_to_append, u32 cols_to_append)
{
    if (!csv || !columns_to_append)
    {
        set_error(ERR_INVALID_ARG);
        return;
    }

    if (rows_to_append != csv->rows_count)
    {
        set_error(ERR_CSV_OUT_OF_BOUNDS);
        return;
    }

    for (u32 col = 0; col < cols_to_append; col++)
    {
        append_column(csv, columns_to_append[col], rows_to_append);
    }
    return;
}

void append_row(CSV *csv, String_View *row_to_append, u32 cols_to_append)
{
    if (!csv || !row_to_append)
    {
        set_error(ERR_INVALID_ARG);
        return;
    }

    if (cols_to_append != csv->cols_count)
    {
        set_error(ERR_CSV_OUT_OF_BOUNDS);
        return;
    }

    // This is mess hahaha
    u64 rows, new_rows;
    rows = csv->rows_count - 1;
    new_rows = rows + 1;
    u64 new_row_index = rows;
    csv->rows_count++;
    csv->rows = (Row *)arena_realloc(&csv->allocator, csv->rows, sizeof(Row *) * (rows), sizeof(Row *) * (new_rows - 1));
    if (!csv->rows)
    {
        set_error(ERR_MEM_ALLOC);
        return;
    }
    csv->rows[new_row_index].cells = row_to_append;
    // Needs check if each cell from new row matches the column type
    return;
}

void append_many_rows(CSV *csv, String_View **rows_to_append, u32 many_rows, u32 many_cols)
{
    if (!csv || !rows_to_append)
    {
        set_error(ERR_INVALID_ARG);
        return;
    }

    if (many_cols != get_col_count(csv))
    {
        set_error(ERR_INCONSISTENT_COLUMNS);
        return;
    }


    for (s64 row = 0; row < many_rows; row++)
    {
        append_row(csv, rows_to_append[row], many_cols);
    }
}


String_View *csv_filter(CSV *csv, String_View column_name, boolean (*predicate)(String_View cell), u64 *out_count)
{
    if (!csv || !predicate || !out_count)
    {
        set_error(ERR_INVALID_ARG);
        return NULL;
    }

    s64 col = get_column_index(&column_name);
    if (col == -1)
    {
        set_error(ERR_INVALID_COLUMN);
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

    String_View *filtered_cells = arena_alloc(&csv->allocator, sizeof(String_View) * (*out_count));
    if (!filtered_cells)
    {
        set_error(ERR_MEM_ALLOC);
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


void csv_mean(CSV *csv, String_View column_name, double *output)
{
    if (!csv || column_name.size == 0 || !output)
    {
        set_error(ERR_INVALID_ARG);
        return;
    }

    s64 col = get_column_index(&column_name);
    if (col == -1)
    {
        set_error(ERR_INVALID_COLUMN);
        return;
    }

    if (csv->type[col] != CSV_TYPE_INTEGER && csv->type[col] != CSV_TYPE_FLOAT)
    {
        set_error(ERR_CSV_DIFF_TYPE);
        return;
    }

    if (get_row_count(csv) == 0)
    {
        set_error(ERR_CSV_EMPTY);
        return;
    }

    double sum = 0.0;
    for (s64 row = 0; row < get_row_count(csv) - 1; row++)
    {
        if (!is_cell_empty(csv->rows[row].cells[col]))
        {
            sum += to_float(csv->rows[row].cells[col]);
        }
    }

    *output = sum / (get_row_count(csv) - 1);
}

static s32 cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

void csv_median(CSV *csv, String_View column_name, double *output)
{
    if (!csv || column_name.size == 0 || !output)
    {
        set_error(ERR_INVALID_ARG);
        return;
    }

    s64 col = get_column_index(&column_name);
    if (col == -1)
    {
        set_error(ERR_INVALID_COLUMN);
        return;
    }

    if ((csv->type[col] != CSV_TYPE_INTEGER && csv->type[col] != CSV_TYPE_FLOAT))
    {
        set_error(ERR_CSV_DIFF_TYPE);
        return;
    }

    u64 row_count = get_row_count(csv) - 1;
    if (row_count == 0)
    {
        set_error(ERR_CSV_EMPTY);
        return;
    }

    double *values = (double *)malloc(row_count * sizeof(double));
    if (!values)
    {
        set_error(ERR_MEM_ALLOC);
        return;
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
        *output = 0.0;
        return;
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
}

void csv_sd(CSV *csv, String_View column_name, double *output)
{
    if (!csv || column_name.size == 0 || !output)
    {
        set_error(ERR_INVALID_ARG);
        return;
    }

    s64 col = get_column_index(&column_name);
    if (col == -1)
    {
        set_error(ERR_INVALID_COLUMN);
        return;
    }

    if ((csv->type[col] != CSV_TYPE_INTEGER && csv->type[col] != CSV_TYPE_FLOAT))
    {
        set_error(ERR_CSV_DIFF_TYPE);
        return;
    }

    u64 row_count = get_row_count(csv) - 1;
    if (row_count == 0)
    {
        set_error(ERR_CSV_EMPTY);
        return;
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
        *output = 0.0;
        return;
    }
    double mean = sum / valid_count;
    *output = sqrt((sum_sq / valid_count) - (mean * mean));
}