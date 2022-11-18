#ifndef PRINTF_H
#define PRINTF_H

#include "core.h"

int aria_printf(const char* fmt, ...);
int aria_vprintf(const char* fmt, va_list args);
int aria_fprintf(FILE* file, const char* fmt, ...);
int aria_vfprintf(FILE* file, const char* fmt, va_list args);
int aria_sprintf(char** buf, const char* fmt, ...);
int aria_vsprintf(char** buf, const char* fmt, va_list args);

char* aria_format(const char* fmt, ...);

#endif
