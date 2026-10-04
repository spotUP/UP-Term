/* The Amiga clipboard as text (IFF FTXT on clipboard.device unit 0): CHRS
 * in Latin-1 for every reader, UTF8 beside it (handler/clipfmt.h). */
#ifndef CLIP_H
#define CLIP_H

/* UTF-8 text in, any length; 1 on success. */
int  clip_write(const char *utf8, long len);
/* The clip's text as UTF-8, NUL-terminated, in memory to FreeVec; *len
 * its length. 0 when the clipboard holds no text. */
char *clip_read(long *len);

#endif
