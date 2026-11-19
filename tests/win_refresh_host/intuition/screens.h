#ifndef WRH_SCREENS_H
#define WRH_SCREENS_H
#include <intuition/intuition.h>
struct Layer_Info { struct SignalSemaphore Lock; };
struct Screen { struct Screen *NextScreen; struct Window *FirstWindow; struct Layer_Info LayerInfo; };
#endif
