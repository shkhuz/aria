#ifndef _ERROR_VALUE_H
#define _ERROR_VALUE_H

typedef enum {
    ERROR_SUCCESS = 0,
    ERROR_READ,
    ERROR_LEX,
    ERROR_PARSE,
} Error;

#endif /* _ERROR_VALUE_H */
