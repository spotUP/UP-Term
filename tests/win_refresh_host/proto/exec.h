#ifndef WRH_PROTO_EXEC_H
#define WRH_PROTO_EXEC_H
#include <exec/types.h>
struct Task *FindTask(STRPTR name);
ULONG AttemptSemaphore(struct SignalSemaphore *s);
void ReleaseSemaphore(struct SignalSemaphore *s);
#endif
