#ifndef WRH_SCREENS_H
#define WRH_SCREENS_H
#include <intuition/intuition.h>
struct Screen { struct Screen *NextScreen; struct Window *FirstWindow; };
#endif
