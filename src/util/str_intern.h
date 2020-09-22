#ifndef _STR_INTERN_H
#define _STR_INTERN_H

#include "types.h"

typedef struct {
	char* str;
	u64 len;
} StrIntern;

char* str_intern_range(char* start, char* end);
char* str_intern(char* str);

#endif /* _STR_INTERN_H */
