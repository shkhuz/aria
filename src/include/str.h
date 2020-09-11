#ifndef _STR_H
#define _STR_H

#include <arpch.h>

typedef struct {
    bool found;
    u64 pos;
} FindCharResult;

FindCharResult find_last_of(const char* str, char c);
char* substr(const char* str, u64 start, u64 end);
char* concat(const char* a, const char* b);

#endif /* _STR_H */
