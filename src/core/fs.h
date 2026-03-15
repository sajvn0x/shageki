#ifndef CORE_FS_H
#define CORE_FS_H

#include "core/defines.h"

bool read_file(const char* filename, u64* out_size, char** out_data);

#endif  // CORE_FS_H
