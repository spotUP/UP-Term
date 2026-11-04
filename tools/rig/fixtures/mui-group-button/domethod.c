/* DoMethod for vbcc without the MUI SDK's amiga.lib: calls the object's class dispatcher
 * (Hook: h_Entry(hook, obj, msg)) with the method id and its arguments on the stack as the message. */
#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <utility/hooks.h>

ULONG DoMethod(Object *obj, ULONG MethodID, ...)
{
    struct IClass *cl;
    if (!obj)
        return 0;
    cl = *(struct IClass **)((UBYTE *)obj - sizeof(struct IClass *));
    return ((ULONG (*)(__reg("a0") struct IClass *, __reg("a2") Object *, __reg("a1") ULONG *))cl->cl_Dispatcher.h_Entry)(cl, obj, &MethodID);
}
