/* The Amiga clipboard as text (IFF FTXT on clipboard.device unit 0): CHRS
 * in Latin-1 for every reader, UTF8 beside it (handler/clipfmt.h). */
#ifndef CLIP_H
#define CLIP_H

/* UTF-8 text in, any length; 1 on success. */
int  clip_write(const char *utf8, long len);
/* A finished IFF FORM file (e.g. a screenshot ILBM) as the clip, streamed from the file in chunks;
 * 1 on success, 0 when the file is no single FORM of its stated length or the device refused. */
int  clip_write_form(const char *path);
/* The clip's text as UTF-8, NUL-terminated, in memory to FreeVec; *len
 * its length. 0 when the clipboard holds no text. */
char *clip_read(long *len);
char *clip_read_raw(long *len);

#endif
