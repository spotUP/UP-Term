/* otag: an outline font's .otag file (FONTS:<name>.otag), checked with no
 * OS calls so it can be host-tested (tests/test_otag.c).
 *
 * The file is a TagItem list as it lies in memory on the Amiga: pairs of
 * big-endian 32-bit (tag, data), ending in TAG_DONE. The first tag is
 * OT_FileIdent, whose data is the file's size. A tag with the OT_Indirect
 * bit (0x8000) holds an offset from the start of the file in place of its
 * value (a string, a table); a reader adds the buffer's address to make it
 * a pointer before handing the list to an engine (OT_OTagList).
 *
 * otag_check walks the list once: the size matches, every indirect offset
 * falls inside the file, the list ends, and the engine's name (OT_Engine,
 * e.g. "ttf" for ttf.library, "bullet" for Intellifont) is a string there.
 *
 * Portable C89, as render/glyphmap.c. */
#ifndef OTAG_H
#define OTAG_H

#define OTAG_TAG_DONE   0UL
#define OTAG_TAG_USER   0x80000000UL
#define OTAG_INDIRECT   0x8000UL
#define OTAG_FILEIDENT  (OTAG_TAG_USER | 0x1000UL | 0x01UL)
#define OTAG_ENGINE     (OTAG_TAG_USER | 0x1000UL | OTAG_INDIRECT | 0x02UL)

/* What otag_check found. */
typedef struct otag_info {
    long ntags;        /* tags before TAG_DONE */
    long engine_off;   /* the engine name's offset in the file */
} otag_info;

enum {
    OTAG_OK = 0,
    OTAG_SHORT,        /* smaller than one tag and TAG_DONE */
    OTAG_NO_IDENT,     /* the first tag is not OT_FileIdent */
    OTAG_SIZE,         /* OT_FileIdent's size is not the file's */
    OTAG_BAD_OFFSET,   /* an indirect value points outside the file */
    OTAG_NO_END,       /* no TAG_DONE inside the file */
    OTAG_NO_ENGINE     /* no OT_Engine, or its name runs off the end */
};

/* buf: the whole file as read, len bytes. */
int otag_check(const unsigned char *buf, long len, otag_info *out);

#endif
