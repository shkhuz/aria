#ifndef MSG_H
#define MSG_H

#include "span.h"

typedef enum {
    MSG_ERROR,
    MSG_WARNING,
    MSG_NOTE,
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
void msg_addl_fat(Msg* m, const char* msg, Span span);
void msg_addl_thin(Msg* m, const char* msg);
// This function is not to be used directly. The module in which it is called
// will define a helper function named `msg_emit` (without the underscore)
// and that makes sure that the necessary flags are set for the module
// to know what occured.
// For example: the `lex` module will define a function named `msg_emit` and
// it's job is to set the `error` flag inside the `LexCtx` if an error has
// occured.
// Exception: `main.c` and some others use this directly because they do not
// have the flags stored in their ctx.
void _msg_emit(Msg* msg);

#endif
