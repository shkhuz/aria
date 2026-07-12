#include "node.h"

const char* nodekind_strs[] = {
    #define WRAP(KIND) #KIND,
    #include "nodekind.def"
    #undef WRAP
};

