#ifndef MSG_H
#define MSG_H

#include "types.h"

typedef enum {
    MSG_KIND_ROOT_ERROR,
    MSG_KIND_ROOT_WARNING,
    MSG_KIND_ROOT_NOTE,
    MSG_KIND_ERROR,
    MSG_KIND_WARNING,
    MSG_KIND_NOTE,
    MSG_KIND_ADDINFO,
} MsgKind;

void addinfo(const char* fmt, ...);
void vaddinfo(const char* fmt, va_list args);
void vmsg(
        MsgKind kind,
        Srcfile* srcfile,
        usize line,
        usize col,
        usize ch_count,
        const char* fmt,
        va_list args);
void msg(
    MsgKind kind,
    Srcfile* srcfile,
    usize line,
    usize col,
    usize ch_count,
    const char* fmt,
    ...);
void note_tok(Token* token, const char* fmt, ...);
void fatal_error_tok(Token* token, const char* fmt, ...);
void error_tok(Token* token, const char* fmt, ...);
void root_note(const char* fmt, ...);

#endif
