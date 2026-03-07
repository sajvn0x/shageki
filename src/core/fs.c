#include "fs.h"

#include <stdlib.h>

char* read_file(const char* filename, size_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = malloc(*size);
    fread(data, 1, *size, f);
    fclose(f);
    return data;
}
