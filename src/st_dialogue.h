#ifndef __STDIALOGUE_H__
#define __STDIALOGUE_H__
    #include "doomtype.h"

    // Only text for now
    typedef struct {
        const char* text;
    } dialogue_t;

    extern dialogue_t testdialogue;
    extern UINT8 indialogue;
    extern dialogue_t* currentdialogue;   
#endif