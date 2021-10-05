#ifndef MSG_H
#define MSG_H

#include "core.h"
#include "aria.h"

typedef enum {
    MSG_KIND_ROOT_ERROR,
    MSG_KIND_ROOT_NOTE,
    MSG_KIND_ERROR,
    MSG_KIND_WARNING,
    MSG_KIND_NOTE,
} MsgKind;

void vdefault_msg(
        MsgKind kind, 
        Srcfile* srcfile, 
        size_t line, 
        size_t column, 
        size_t char_count, 
        const char* fmt,
        va_list ap);

void default_msg(
        MsgKind kind, 
        Srcfile* srcfile, 
        size_t line, 
        size_t column, 
        size_t char_count, 
        const char* fmt,
        ...);

void terminate_compilation();

void default_msg_from_tok(
        MsgKind kind,
        Token* token,
        const char* fmt,
        ...);

void error(
        Token* token,
        const char* fmt, 
        ...);

void fatal_error(
        Token* token,
        const char* fmt,
        ...);

void warning(
        Token* token,
        const char* fmt, 
        ...);

void note(
        Token* token,
        const char* fmt, 
        ...);

#endif
