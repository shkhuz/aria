#ifndef _ERROR_RECOVER_H
#define _ERROR_RECOVER_H

#include "util/util.h"

#define es \
    u64 COMBINE(_error_store,__LINE__) = self->error_count

#define er \
    if (self->error_count > COMBINE(_error_store,__LINE__)) return

#define ec \
    if (self->error_count > COMBINE(_error_store,__LINE__)) continue

#define ern \
    er null

#define chk(stmt) \
    es; stmt; ern;

#define chkv(stmt) \
    es; stmt; er;

#define chklp(stmt) \
    es; stmt; ec;

#endif /* _ERROR_RECOVER_H */
