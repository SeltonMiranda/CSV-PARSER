#include <stdio.h>
#include "csvParser.h"
#include <stdlib.h>
#include <string.h>



int main() {
    CSV csv;
    init_csv(&csv);
    read_csv("username.csv", &csv);
    if (error())
    {
        return 1;
    }

    printf("--------------------------------------------------------------------------------\n");
    print_csv(&csv);

    deinit_csv(&csv);
    return 0;
}