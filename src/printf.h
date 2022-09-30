#ifndef PRINTF_H
#define PRINTF_H

#include "core.h"

int aprintf(const char* fmt, ...);
int avprintf(const char* fmt, va_list args);
int afprintf(FILE* file, const char* fmt, ...);
int avfprintf(FILE* file, const char* fmt, va_list args);
int asprintf(char** buf, const char* fmt, ...);
int avsprintf(char** buf, const char* fmt, va_list args);

#endif
