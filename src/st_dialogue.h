#ifndef __STDIALOGUE_H__
#define __STDIALOGUE_H__
    #include "doomtype.h"

    // Only text for now
    typedef struct dialogue_s {
        const char* text;
        struct dialogue_s *next;
    } dialogue_t;

    extern dialogue_t testdialogue;
    extern UINT8 indialogue;
    extern dialogue_t* currentdialogue;   
#endif