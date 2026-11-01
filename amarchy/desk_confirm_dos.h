/* FP-1 (R1.8): the confirm requester of the native desktop theme, in the commodity. desk_confirm (desk_core.h) is the state machine; this
 * file is the requester (BuildEasyRequest + SysReqHandler: never a blocking EasyRequest, the Wait loop keeps running) and the 1 s tick
 * (the commodity tick hook) that carries the 15 s timeout. */
#ifndef AMARCHY_DESK_CONFIRM_DOS_H
#define AMARCHY_DESK_CONFIRM_DOS_H

/* installs desk_dos_confirm_begin and the tick hook; call once at start, after tick_open */
void dc_init(void);
/* the ARexx verb DESK CONFIRM <mask> (from the process that applied): same as the direct hook */
void dc_begin(unsigned mask);
/* the requester's window signal mask (0 when none is open); add it to the Wait mask and call dc_serve when it fires */
unsigned long dc_sigmask(void);
void dc_serve(void);
/* counters published as ENV: variables AMARCHY_CONFIRM_SHOWN / _KEEPS / _REVERTS_USER / _REVERTS_TIMEOUT */
extern unsigned long dc_shown, dc_keeps, dc_reverts_user, dc_reverts_timeout;

#endif
