#include <stdio.h>
#include "core.h"
int main() {
    int* buf = NULL;
    bufpush(buf, 42);
    printf("%d", buflen(buf));
}
