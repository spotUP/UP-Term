/* prefs_asl: the asl.library requesters UP-Term uses, in one place -- the
 * handler's Tab on an empty word (file), Settings > Font... (font),
 * Settings > Theme... (theme file), and UP-Term Prefs' Font, Fallback font,
 * Theme... and Screen mode buttons. Each opens asl.library for the call
 * (V37; the screen mode requester needs V38) and closes it again.
 *
 * win, when not 0, is the parent window (the requester opens on its screen
 * and the window sleeps meanwhile); otherwise scr is the screen. Every
 * function answers 1 with the result in the caller's buffers, 0 when the
 * user cancelled, asl.library is not there or the requester could not be
 * made. */
#ifndef PREFS_ASL_H
#define PREFS_ASL_H

struct Screen;
struct Window;

/* A file requester: drawer, pattern ("#?.conf", 0: all files), icons
 * rejected when reject_icons. out: drawer + file as one path (cap bytes);
 * *file_named (when not 0) tells whether a file was named (0: only a drawer). */
int asl_pick_file(struct Screen *scr, struct Window *win, const char *title,
                  const char *drawer, const char *pattern, int reject_icons,
                  char *out, int cap, int *file_named);

/* A font requester, name ("topaz.font", "" none) and size preselected; only
 * fixed-width fonts when fixed_only. The chosen ta_Name in out_name (cap
 * bytes), its ta_YSize in *out_size. */
int asl_pick_font(struct Screen *scr, struct Window *win, const char *title,
                  const char *name, long size, int fixed_only,
                  char *out_name, int cap, long *out_size);

/* The screen mode requester with the depth gadget (min_depth..max_depth),
 * starting at display mode id (0: the requester's own choice) and depth. Opens over win's screen, else on scr
 * (0: the default public screen). */
int asl_pick_screenmode(struct Screen *scr, struct Window *win, const char *title,
                        unsigned long id, int depth, int min_depth, int max_depth,
                        unsigned long *out_id, int *out_depth);

/* The display database's name of display mode id ("PAL:High Res"), into out (cap bytes, always terminated).
 * 1 when the mode has a name, 0 and "" when it has none (or graphics.library is not there): the caller shows
 * the hex id instead. */
int asl_mode_name(unsigned long id, char *out, int cap);

#endif
