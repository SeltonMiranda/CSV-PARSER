#include <stdio.h>
#include "csvParser.h"
#include <stdlib.h>

int main() {
    CSV csv;
    init_csv(&csv);
    if (!read_csv("username.csv", &csv))
    {
        printf("An error occurred\n");
        return 1;
    }
    
    //printf("rows: %lu, cols: %lu\n", get_row_count(&csv), get_col_count(&csv));
    print_csv(&csv);

    u8 *new_column[][6] = {
        {"CPF", "999999.9", "888888", "777777", "666666", "555555"},
        {"Age", "101", "32", "41", "15", "76"},
        {"Phone", "9999-9999", "888-888", "777-777", "666-666", "555-555"}
    };

    u8 ***ptr = malloc(sizeof(u8 **) * 3);
    for (size_t i = 0; i < 3; i++)
    {
        ptr[i] = new_column[i];
    }

    if (!append_many_columns(&csv, ptr, 6, 3))
    {
        printf("not possible\n");
        return 1;
    }
    free(ptr);

    printf("--------------------------------------------------------------------------------\n");
    print_csv(&csv);
    
    deinit_csv(&csv);
    return 0;
}