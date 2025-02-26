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

    //printf("--------------------------------------------------------------------------------\n");
    //print_csv(&csv);
    const String_View *column = get_column(&csv, (String_View){ .data = "Identifier", .size = strlen("Identifier")});
    if (column == NULL)
    {
        if (error())
        {
            return 1;
        }
    }

    for (size_t i = 0; i < get_row_count(&csv); i++)
    {
        printf(sv_fmt"\n", sv_args(column[i]));
    }

    deinit_csv(&csv);
    return 0;
}