#ifndef __G_BATTLE__
#define __G_BATTLE__

#include "p_mobj.h"
#include "m_menu.h"

typedef enum {
    MOVEANIM_NONE = 0, // No move animation playing
    MOVEANIM_STROLL, // Placeholder / extremely generic move
    NUMMOVEANIMS
} moveanim_t;

typedef enum {
    ITEM_NONE = 0,
    ITEM_CHILIDOG,
    ITEM_PIKACHUHEAD,
    NUMITEMTYPES
} itemtype_t;

typedef struct {
    itemtype_t item;
    UINT8 quantity;
} item_t;

extern boolean battle;
extern boolean canmove;
extern menu_t* currentbattlemenu;
extern UINT8 itemon_battle;
extern mobj_t* battletarget;
extern boolean specialbattlemenu;

extern UINT8 moveanim_step; // Which step of the move animation are we at?
extern moveanim_t moveanim; // WHICH move animation is playing?
extern mobj_t* moveanim_source;
extern mobj_t* moveanim_target;

extern item_t inventory[50];
extern UINT8 numitems;

extern char* itemnames[NUMITEMTYPES];
#endif