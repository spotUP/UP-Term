/* The Amiga clipboard as text (IFF FTXT/CHRS on clipboard.device unit 0). */
#ifndef CLIP_H
#define CLIP_H

/* Latin-1 text in, 1 on success. */
int  clip_write(const char *text, long len);
/* The first CHRS chunk of the clip, NUL-terminated; returns its length. */
long clip_read(char *out, long max);

#endif
