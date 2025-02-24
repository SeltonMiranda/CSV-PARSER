#include <stdio.h>
#include "csvParser.h"
#include <stdlib.h>
#include <string.h>

boolean filter_above_5000(String_View cell)
{
    return to_integer(cell) >= 5000;
}

boolean filter_empty_cells(String_View cell)
{
    return is_cell_empty(cell) ? TRUE : FALSE;
}

int main() {
    CSV csv;
    init_csv(&csv);
    if (!read_csv("username.csv", &csv))
    {
        printf("An error occurred\n");
        return 1;
    }
    String_View *column = malloc(sizeof(String_View) * get_row_count(&csv));
    for (size_t i = 0; i < get_row_count(&csv); i++)
    {
        column[i] = sv("Hello World");
    }

    if (!append_column(&csv, column, get_row_count(&csv)))
    {
        printf("An error occurred\n");
        return 1;
    }

    printf("--------------------------------------------------------------------------------\n");
    print_csv(&csv);
    save_csv(NULL, &csv);
    
    deinit_csv(&csv);
    return 0;
}