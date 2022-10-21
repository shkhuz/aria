#include "types.h"
#include "buf.h"

StringIntMap* keywords = NULL;

void init_types() {
    bufpush(keywords, (StringIntMap){ "imm", 0 });
    bufpush(keywords, (StringIntMap){ "mut", 0 });

    bufloop(keywords, i) {
        keywords[i].v = strlen(keywords[i].k);
    }
}
