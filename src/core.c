#include "core.h"
#include "aria.h"

void* aria_malloc(size_t bytes) {
    return malloc(bytes);
}

void* aria_realloc(void* buf, size_t bytes) {
    return realloc(buf, bytes);
}

void aria_free(void* buf) {
    return free(buf);
}
