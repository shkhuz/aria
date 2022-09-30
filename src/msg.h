#ifndef MSG_H
#define MSG_H

#include "types.h"

typedef enum {
    MSG_KIND_ROOT_ERROR,
    MSG_KIND_ROOT_NOTE,
    MSG_KIND_ERROR,
    MSG_KIND_WARNING,
    MSG_KIND_NOTE,
    MSG_KIND_ADDINFO,
} MsgKind;

void addinfo(const char* fmt, ...);

void msg(
    MsgKind kind,
    Srcfile* srcfile,
    usize line,
    usize col,
    usize ch_count,
    const char* fmt,
    ...);

#endif
