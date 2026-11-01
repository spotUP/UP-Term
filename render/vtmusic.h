/* vtmusic: ANSI music notes (engine vt_note) to audio.device, never waiting.
 * The engine parses the string (engine/ansimusic.c); this plays it. */
#ifndef VTMUSIC_H
#define VTMUSIC_H
#include "../engine/vtengine.h"

/* Queue the notes on one audio channel and return at once. background is
 * the MB flag; both forms queue without blocking, so output never stalls.
 * The device opens on the first call; a full queue drops notes. */
void vtmusic_play(const vt_note *notes, int n, int background);
/* Stop every queued note and close the device (the last window went). */
void vtmusic_close(void);
#endif
