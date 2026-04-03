#ifndef __G_BATTLE__
#define __G_BATTLE__

#include "p_mobj.h"
#include "m_menu.h"

typedef enum {
    MOVEANIM_NONE = 0, // No move animation playing
    MOVEANIM_STROLL, // Placeholder / extremely generic move
    NUMMOVEANIMS
} moveanim_t;

extern boolean battle;
extern boolean canmove;
extern menu_t* currentbattlemenu;
extern UINT8 itemon_battle;
extern mobj_t* battletarget;

extern UINT8 moveanim_step; // Which step of the move animation are we at?
extern moveanim_t moveanim; // WHICH move animation is playing?
extern mobj_t* moveanim_source;
extern mobj_t* moveanim_target;

#endif