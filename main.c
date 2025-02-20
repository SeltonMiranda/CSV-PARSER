#include <stdio.h>
#include "csvParser.h"

int main() {
    CSV csv;
    init_csv(&csv);
    if (!read_csv("username.csv", &csv))
    {
        printf("An error occurred\n");
        return 1;
    }
    
    long int integer_column[get_row_count(&csv) - 1];
    int idx = get_column_index(&csv, "sal");
    if (idx == -1)
    {
        printf("Unknown column name\n");
        return 1;
    }

    if (!convert_column_to_integer(&csv, idx, integer_column))
    {
        printf("cannot convert\n");
        return 1;
    }

    for (size_t i = 0; i < get_row_count(&csv) - 1; i++)
    {
        printf("value : %ld\n", integer_column[i]);
    }

    deinit_csv(&csv);
    return 0;
}