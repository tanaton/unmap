#include "unmap.h"
#include "fnv64.h"

/* hash値生成 */
unmap_hash_t unmap_hash_create(const char *str, size_t key_size)
{
	size_t i = 0;
	unmap_hash_t hash = 14695981039346656037UL;
	for(i = 0; i < key_size; i++){
		hash *= 1099511628211UL;
		hash ^= str[i];
	}
	return hash;
}

