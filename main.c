#include <stdio.h>
#include "csvParser.h"
#include <stdlib.h>
#include <string.h>



int main() {
    CSV csv;
    init_csv(&csv);
    read_csv("tips.csv", &csv);
    if (error())
    {
        return 1;
    }

    printf("--------------------------------------------------------------------------------\n");
    print_csv(&csv);
    save_csv(NULL, &csv);

    deinit_csv(&csv);
    return 0;
}