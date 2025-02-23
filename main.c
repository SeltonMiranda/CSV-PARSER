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
    double mean;
    if (!csv_sd(&csv, sv("Identifier"), &mean))
    {
        printf("error\n");
        return 1;
    }
    printf("mean: %.5f\n", mean);
    printf("--------------------------------------------------------------------------------\n");
    print_csv(&csv);
    
    deinit_csv(&csv);
    return 0;
}