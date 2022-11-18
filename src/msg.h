#ifndef MSG_H
#define MSG_H

#include "types.h"

typedef enum {
    MSG_KIND_ERROR,
    MSG_KIND_WARNING,
    MSG_KIND_NOTE,
} MsgKind;

typedef struct {
    const char* msg;
    Span span;
} SubMsgFat;

typedef struct {
    const char* msg;
} SubMsgThin;

typedef struct {
    MsgKind kind;
    const char* msg;
    OptionalSpan span;
    SubMsgFat* addl_fat;
    SubMsgThin* addl_thin;
} Msg;

Msg msg_with_span(MsgKind kind, const char* msg, Span span);
Msg msg_with_no_span(MsgKind kind, const char* msg);
void msg_emit(Msg* msg);

#endif
