#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unmap.h"

int main()
{
	size_t i = 0;
	char str[9] = {0};
	char *p = 0;
	unmap_t *map = unmap_init();
	for(i = 0; i < 10000000; i++){
		sprintf(str, "%08lu", i);
		p = malloc(9);
		memcpy(p, str, 9);
		unmap_set(map, str, 8, p);
	}
	for(i = 0; i < 10000000; i++){
		sprintf(str, "%08lu", i);
		p = unmap_get(map, str, 8);
		if(strcmp(str, p)){
			printf("%s - %s\n", str, p);
		}
		free(p);
	}
	return 0;
}

