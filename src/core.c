#include "core.h"
#include "aria.h"
#include "buf.h"

void* aria_malloc(size_t bytes) {
    return malloc(bytes);
}

void* aria_realloc(void* buf, size_t bytes) {
    return realloc(buf, bytes);
}

void aria_free(void* buf) {
    return free(buf);
}

char* aria_strsub(char* str, size_t start, size_t len) {
    if (!str) {
        return null;
    }

    if (len == 0) {
        return null;
    }

    char* res = null;
    for (size_t i = start; i < start + len; i++) {
        buf_push(res, str[i]);
    }
    buf_push(res, '\0');
    return res;
}

char* aria_strapp(char* to, char* from) {
    if (!to || !from) {
        return null;
    }

    char* res = null;
    size_t to_len = strlen(to);
    size_t from_len = strlen(from);
    for (size_t i = 0; i < to_len; i++) {
        buf_push(res, to[i]);
    }
    for (size_t i = 0; i < from_len; i++) {
        buf_push(res, from[i]);
    }
    buf_push(res, '\0');
    return res;
}

char* aria_basename(char* path) {
    if (!path) {
        return null;
    } 

    char* last_dot = strrchr(path, '.');
    if (last_dot == null) {
        return path;
    }

    return aria_strsub(path, 0, last_dot - path);
}

char* aria_notdir(char* path) {
    if (!path) {
        return null;
    } 

    size_t path_len = strlen(path);
    char* last_fslash = strrchr(path, '/');
    if (last_fslash == null) {
        return path;
    }

    return aria_strsub(path, last_fslash - path + 1, path_len);
}   

char* aria_dir(char* path) {
    if (!path) {
        return null;
    } 

    char* last_fslash = strrchr(path, '/');
    if (last_fslash == null) {
        return null;
    }

    return aria_strsub(path, 0, last_fslash - path + 1);
}

size_t align_to_pow2(size_t n, size_t pow2) {
	if (pow2 == 0) return n;
    return (n + (pow2-1)) & ~(pow2-1);
}

size_t get_bits_for_value(u128 n) {
	size_t count = 0;
	while (n > 0) {
		n = n >> 1;
		count++;
	}
	return count;
}

int char_to_digit(char c) {
    return c - 48;
}
