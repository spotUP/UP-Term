/* Amarchy clipboard history, the portable part (CLIP-HISTORY, like Omarchy's cliphist). No OS calls
 * except stdio for the ring file, so the vttest_host suite "clips" covers it. The commodity
 * (amarchy/clipwatch.c) feeds the ring from clipboard.device and saves it to CLIPS_PATH; the index
 * loader (index_load.c) reads it back and the drawn menu shows it as the level `trigger.clipboard`.
 *
 * The ring holds at most CLIPS_MAX text entries, newest first. Ownership: every entry is a heap copy
 * (malloc, NUL-terminated, len bytes) owned by the ring; clips_free releases them and zeroes the ring.
 * A ring starts as memset 0 (static, or clips_init) and may be reloaded: clips_load_file and
 * clips_parse free what it held first. An entry longer than CLIPS_TEXT bytes is cut (at a UTF-8
 * boundary) when it is prepended; an entry stops at its first NUL.
 *
 * File format (clips_serialize / clips_parse), one record per entry, newest first:
 *     <decimal byte length> LF <that many bytes> LF
 * The length prefix lets an entry hold newlines, tabs or any byte but NUL. A record whose length is
 * missing, too long or past the end of the text ends the parse: the entries before it are kept. */
#ifndef AMARCHY_CLIPS_CORE_H
#define AMARCHY_CLIPS_CORE_H

#define CLIPS_MAX     20
#define CLIPS_TEXT    2047          /* most bytes of one entry */
#define CLIPS_LABEL   40            /* most characters of a row label */
#define CLIPS_PATH    "ENV:Amarchy/clips"

typedef struct clip_ring {
    int   n;                        /* entries in use, 0..CLIPS_MAX */
    char *text[CLIPS_MAX];          /* text[0] is the newest; heap, NUL-terminated */
    long  len[CLIPS_MAX];           /* bytes of text[i], without the NUL */
} clip_ring;

void clips_init(clip_ring *r);      /* an empty ring; the same as memset 0 */
void clips_free(clip_ring *r);      /* free every entry, leave the ring empty */

/* Put text (len bytes) at the front. Empty text (len 0, or a NUL first) is ignored; an entry equal to
 * one already held moves to the front instead of appearing twice; the 21st entry pushes the oldest
 * out. 1 when the ring changed, else 0 (empty text, an equal entry already first, or no memory). */
int clips_prepend(clip_ring *r, const char *text, long len);

/* One line for a menu row: the first CLIPS_LABEL characters, each control character (below 0x20, and
 * 0x7F) shown as a space. UTF-8 stays whole: a sequence is never cut, so a label that would not fit n
 * bytes ends before it (n of CLIPS_LABEL * 4 + 1 always holds 40 characters). out is NUL-terminated. */
void clips_label(const char *text, long len, char *out, int n);

/* The ring as one malloc'd block (see the file format), its length in *len; 0 on no memory.
 * The caller frees it. No NUL is added: the length is the end. */
char *clips_serialize(const clip_ring *r, long *len);

/* The ring from a text block of len bytes, replacing what it held; the entries read. */
int clips_parse(clip_ring *r, const char *text, long len);

/* The ring from the file at path (a missing or unreadable file gives an empty ring); the entries. */
int clips_load_file(clip_ring *r, const char *path);

/* Write the ring to the file at path; 1 on success, 0 on failure. */
int clips_save_file(const clip_ring *r, const char *path);

#endif
