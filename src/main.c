#include "core.h"
#include "buf.h"
#include "printf.h"
#include "bigint.h"
#include "file_io.h"

int main() {
    /* int* buf = NULL; */
    /* bufpush(buf, -2); */
    /* bufpush(buf, -3); */
    /* bufpush(buf, 4); */
    /* bufpush(buf, 2948); */

    /* for (usize i = 0; i < buflen(buf); i++) { */
    /*     aprintf("%d\n", buf[i]); */
    /* } */
    /* aprintf("%lu is a number with /%s/", UINT64_MAX, "www"); */
    /* buffree(buf); */

    /* bigint a; */
    /* bigint_init_u64(&a, 1); */
    /* bigint_set_u64(&a, 1); */

    FileOrError file = read_file("src/main.c");
    assert(file.status == FOO_SUCCESS);
    aprintf("%s", file.handle.contents);
}
