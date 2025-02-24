#include <stdio.h>
#include "csvParser.h"
#include <stdlib.h>
#include <string.h>



int main() {
    CSV csv;
    
    init_csv(&csv);
    if (!read_csv("username.csv", &csv))
    {
        printf("An error occurred\n");
        return 1;
    }
    fillna(&csv);

    printf("--------------------------------------------------------------------------------\n");
    print_csv(&csv);

    CSV csv2 = dropna(&csv);
    if (is_csv_empty(&csv2))
    {
        printf("csv vazio\n");
        return 1;
    }

    printf("--------------------------------------------------------------------------------\n");



    deinit_csv(&csv);
    print_csv(&csv2);
    deinit_csv(&csv2);
    return 0;
}