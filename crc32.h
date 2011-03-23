#ifndef UNMAP_CRC32_H_INCLUDE
#define UNMAP_CRC32_H_INCLUDE

#include <string.h>

#define UNMAP_CRC32_TABLE_SIZE		(256)

extern unmap_box_t unmap_hash_create(const char *str, size_t size, size_t max_level);

#endif /* UNMAP_CRC32_H_INCLUDE */

