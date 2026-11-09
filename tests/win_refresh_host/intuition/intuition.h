/* host model: the window fields win_refresh.c reads */
#ifndef WRH_INTUITION_H
#define WRH_INTUITION_H
#include <exec/types.h>
struct Window { struct Window *NextWindow; struct MsgPort *UserPort; };
#endif
