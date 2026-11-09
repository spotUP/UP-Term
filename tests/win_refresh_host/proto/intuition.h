/* host model: IntuitionBase and the three calls win_refresh.c makes; the test defines them */
#ifndef WRH_PROTO_INTUITION_H
#define WRH_PROTO_INTUITION_H
#include <intuition/intuitionbase.h>
extern struct IntuitionBase *IntuitionBase;
ULONG LockIBase(ULONG dontknow);
void UnlockIBase(ULONG lock);
void RefreshWindowFrame(struct Window *w);
#endif
