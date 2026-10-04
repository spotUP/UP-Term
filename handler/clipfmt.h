/* clipfmt: the clipboard's text formats, portable C89 (host-tested; the
 * device IO is clip.c).
 *
 * The Amiga clipboard holds text as IFF FTXT. Its CHRS chunk is 8-bit text
 * in the system character set -- ISO 8859-1 on AmigaOS 3.x, what ConClip,
 * the Shell, Ed and every older program read and write. Unicode travels in
 * a UTF8 chunk beside it (AmigaOS wiki, "UTF8 IFF UTF-8 Unicode Text":
 * writers may put both in the FORM, readers that know UTF8 take it and
 * ignore CHRS, older readers take CHRS; chunk order is not fixed). So a
 * copy writes both, CHRS first, and a paste takes UTF8 when there is one.
 * The CSET chunk (FTXT's own character-set field) is not used: the wiki
 * calls it impractical and no reader we know honours it. */
#ifndef CLIPFMT_H
#define CLIPFMT_H

/* UTF-8 to Latin-1, as CHRS holds it: a code point beyond Latin-1 is '?',
 * a byte that starts no valid sequence is taken as Latin-1 itself. out may
 * be in (Latin-1 is never longer). Returns the length. */
long cf_to_latin1(const char *in, long n, char *out);
/* Latin-1 to UTF-8; out has room for 2 * n bytes. Returns the length. */
long cf_from_latin1(const char *in, long n, char *out);
/* The code point at s[*i] of the n bytes of UTF-8, *i moved past it (a
 * byte that starts no valid sequence is that byte, as Latin-1). */
unsigned long cf_next(const char *s, long n, long *i);
/* A pasted character is typed, or dropped: controls other than TAB, LF
 * and CR are dropped, C1 too -- an ESC in the clipboard would otherwise
 * end a bracketed paste early (ESC [ 201 ~) and run the rest as typed
 * commands, or send the terminal sequences of its own. */
int cf_paste_keeps(unsigned long cp);

/* The FTXT FORM for a copy: its 20-byte head (FORM, size, FTXT, CHRS,
 * size) for chrs bytes of CHRS, and the chunk head that follows the CHRS
 * data (its pad byte, then UTF8, size): returns that tail's length (9 or
 * 8). The UTF8 data, and its pad byte when utf8 is odd, close the FORM. */
void cf_ftxt_head(unsigned char *head, long chrs, long utf8);
int  cf_ftxt_mid(unsigned char *mid, long chrs, long utf8);

/* Reads an FTXT clip through rd (bytes in order, as clipboard.device
 * gives them; returns the count, < n at the end, -1 on an error) and
 * returns its text as UTF-8, NUL-terminated, in memory from alloc (free it
 * with release's partner): the UTF8 chunk when the FORM has one, else the
 * first CHRS read as Latin-1. *len gets the length. 0 when the clip holds
 * no text (or alloc failed). Reads no further than the FORM. */
typedef long (*cf_read_fn)(void *u, void *buf, long n);
char *cf_read_ftxt(cf_read_fn rd, void *u, void *(*alloc)(unsigned long), void (*release)(void *),
                   long *len);

#endif
