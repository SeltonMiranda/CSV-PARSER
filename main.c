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
    
    fillna(&csv);

    printf("--------------------------------------------------------------------------------\n");
    print_csv(&csv);
    
    deinit_csv(&csv);
    return 0;
}