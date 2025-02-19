#include <stdio.h>
#include "csvParser.h"

int main() {
    CSV csv;
    init_csv(&csv);
    read_csv("username.csv", &csv);

    deinit_csv(&csv);
    return 0;
}