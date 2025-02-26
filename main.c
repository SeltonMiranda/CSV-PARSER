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

    //printf("--------------------------------------------------------------------------------\n");
    //print_csv(&csv);
    const String_View *column = get_column(&csv, (String_View){ .data = "\"size\"", .size = strlen("\"size\"")});
    if (column == NULL)
    {
        if (error())
        {
            return 1;
        }
    }

    print_column(column, get_row_count(&csv));

    deinit_csv(&csv);
    return 0;
}