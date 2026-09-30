/* devwho -- is console.device UP-Term's, and what has it done? DV1 of the
 * console.device plan: opens console.device CONU_LIBRARY and sends the
 * private UPCMD_STATS, which only UP-Term's device answers. Prints
 * "UP-Term units U written W answered A dropped D" or "ROM error N". */
#include <stdio.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <proto/exec.h>
#include "../../device/upc_public.h"

int main(void)
{
    struct IOStdReq io;
    struct upc_stats st;
    struct MsgPort *p = CreateMsgPort();
    if (!p)
        return 20;
    io.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    io.io_Message.mn_ReplyPort = p;
    io.io_Message.mn_Length = sizeof(io);
    if (OpenDevice((STRPTR)"console.device", (ULONG)CONU_LIBRARY, (struct IORequest *)&io, 0)) {
        printf("no console.device\n");
        DeleteMsgPort(p);
        return 20;
    }
    if (!upc_is_upterm((struct Library *)io.io_Device)) {
        /* no private command to a device that is not ours */
        printf("ROM error -3\n");
    } else {
        io.io_Command = UPCMD_STATS;
        io.io_Data = &st;
        io.io_Length = sizeof(st);
        if (DoIO((struct IORequest *)&io) == 0)
            printf("UP-Term units %lu written %lu answered %lu dropped %lu events %lu mice %lu drags %lu "
                   "pointer %lu,%lu\n", st.units, st.written, st.answered, st.dropped, st.events, st.mice,
                   st.drags, st.pointer >> 16, st.pointer & 0xFFFF);
        else
            printf("UP-Term error %d\n", io.io_Error);
    }
    CloseDevice((struct IORequest *)&io);
    DeleteMsgPort(p);
    return 0;
}
