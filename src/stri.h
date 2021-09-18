#ifndef STRI_H
#define STRI_H

#include "core.h"

typedef struct {
    char* str;
    size_t len;
} StrIntern;

char* strni(char* start, char* end);
char* stri(char* str);

#endif
