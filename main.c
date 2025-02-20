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

    printf("rows: %lu cols: %lu\n", get_row_count(&csv), get_col_count(&csv));
    u8 **row_one = get_row_at(&csv, 10);
    if (!row_one)
    {
        printf("Failed to get row index %u\n", 10);
    }
    else
    {
        for (size_t i = 0; i < get_col_count(&csv); i++)
        {
            printf("%s ", row_one[i]);
        }
        printf("\n");
    }

    long int value;
    convert_cell_to_integer(&csv, 0, 1, &value);
    printf("value = %lu\n", value);


    deinit_csv(&csv);
    return 0;
}