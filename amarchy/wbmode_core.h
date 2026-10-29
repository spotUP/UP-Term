/* W43: Amarchy opens no window until the Workbench is in its final screen mode. At boot IPrefs resets the Workbench
 * to the saved mode (ENV:Sys/screenmode.prefs) late, after the RTG monitor loads; Intuition cannot reset while a
 * foreign window is open (it asks to close all windows), so the Workbench stayed PAL 640x256. The pure half: the
 * prefs IFF parser and the wait decision. wbmode.c reads the file and the screen and polls on the 1 s tick.
 * Host-tested by tests/test_wbmode.c. */
#ifndef AMARCHY_WBMODE_CORE_H
#define AMARCHY_WBMODE_CORE_H

#define WBM_INVALID_ID 0xFFFFFFFFUL
#define WBM_TIMEOUT_TICKS 30L      /* knob: seconds (1 s ticks) to wait for the mode before opening anyway */

enum { WBM_WAIT = 0, WBM_READY = 1, WBM_TIMEOUT = 2 };

typedef struct wbm_prefs {
    unsigned long id;              /* sm_DisplayID */
    unsigned width, height, depth;
} wbm_prefs;

/* FORM <size> PREF, then a chunk walk to SCRM (sm_Reserved[4], DisplayID, Width, Height, Depth, Control).
 * 1 and *out filled; 0 for a short buffer, a wrong FORM/PREF header or no SCRM chunk of 28 bytes. */
int wbm_parse_prefs(const unsigned char *buf, long len, wbm_prefs *out);

/* READY: no prefs file, a prefs ID of INVALID_ID, or the Workbench mode ID equals the prefs ID.
 * Else WAIT while elapsed < timeout, then TIMEOUT (the caller opens its windows anyway and logs one line). */
int wb_mode_ready(unsigned long prefs_id, int have_prefs, unsigned long wb_mode_id, long elapsed_ticks,
                  long timeout_ticks);

#endif
