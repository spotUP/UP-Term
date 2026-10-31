/* screennotify.library, the calls the handler uses: our own declarations
 * from the library's fd (_ScreenNotifyBase, bias 30) and its documented
 * message. The library and its headers are (c) 1995 Stefan Becker and may
 * not be shipped in other packages, so neither is in the repository: the
 * rigs fetch the pinned Aminet archive (tools/rig/rig.py), and the handler
 * opens the library only when it is there (vtcon_handler.c). Amiga only.
 */
#ifndef UP_SCREENNOTIFY_H
#define UP_SCREENNOTIFY_H

#include <exec/ports.h>

#define SCREENNOTIFY_NAME "screennotify.library"
#define SCREENNOTIFY_VERSION 1

#define SCREENNOTIFY_TYPE_WORKBENCH 3 /* snm_Value: FALSE = closing, TRUE = opened or close failed */

struct ScreenNotifyMessage {
    struct Message snm_Message;
    ULONG snm_Type;
    APTR snm_Value;
};

extern struct Library *ScreenNotifyBase;

/* vbcc register calls, in the form of its inline/*_protos.h */
APTR __AddWorkbenchClient(__reg("a6") void *, __reg("a0") struct MsgPort *port, __reg("d0") LONG pri) = "\tjsr\t-54(a6)";
#define AddWorkbenchClient(port, pri) __AddWorkbenchClient(ScreenNotifyBase, (port), (pri))

/* FALSE while the library is sending: the caller retries after a Delay */
BOOL __RemWorkbenchClient(__reg("a6") void *, __reg("a0") APTR handle) = "\tjsr\t-60(a6)";
#define RemWorkbenchClient(handle) __RemWorkbenchClient(ScreenNotifyBase, (handle))

#endif
