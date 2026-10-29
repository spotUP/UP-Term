/* note: the Amarchy notice port (roadmap 2026-10-29-amarchy-p4-capture-notify-power.md R4.9a). A second ARexx host,
 * AMARCHY.NOTIFY, feeds the one queue (note_core). There is no window yet (P2): notices are counted and held. */
#ifndef AMARCHY_NOTE_H
#define AMARCHY_NOTE_H

#define NOTE_PORT "AMARCHY.NOTIFY"

void note_open(void);                   /* make the port and the queue; the commodity runs without it when the name is taken */
unsigned long note_sigmask(void);       /* for the Wait loop; 0 when there is no port */
void note_serve(void);                  /* answer what the port holds */
void note_close(void);                  /* take the port away; queued messages are answered */
void note_expire(long tick_secs);       /* tick hook (tick.h): remove the notices whose time is up */
long note_posted(void);                 /* notices accepted since the start (STATUS notes=) */
long note_run(const char *cmd, char *out);   /* a verb from anywhere else in Amarchy (NOTES on AMARCHY, menu rows): same code as the port */

#endif
