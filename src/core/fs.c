#include "fs.h"

#include <stdio.h>

#include "core/logger.h"
#include "core/memory.h"

bool read_file(const char* filename, u64* out_size, char** out_data) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        LOG_ERROR("File not found: %s", filename);
        return false;
    };

    fseek(f, 0, SEEK_END);
    *out_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out_data = memory_allocate(*out_size, MEMORY_TAG_STRING);
    fread(*out_data, 1, *out_size, f);
    fclose(f);

    return true;
}
