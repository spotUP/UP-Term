/* The commodity's clipboard history (Omarchy's cliphist): a CBD_CHANGEHOOK on clipboard.device unit 0
 * signals the commodity's task; cw_changed then reads the clip (handler/clip.c clip_read) and prepends it to
 * the ring in CLIPS_PATH (clips_core.h). */
#ifndef AMARCHY_CLIPWATCH_H
#define AMARCHY_CLIPWATCH_H

/* Install the hook; 0 when clipboard.device cannot be opened (the history is then off). */
int cw_start(void);
/* The signal mask the hook raises, 0 when it is not running. */
unsigned long cw_sigmask(void);
/* The clipboard changed (the signal came): read it and update the ring file. 1 when the ring changed. */
int cw_changed(void);
/* Remove the hook and free everything. */
void cw_stop(void);

#endif
