/* iconfile_core -- the .info file rewrite of the Lucide icons (plan 2026-10-31-amarchy-lucide-icons.md, L4 and L13).
 * Pure bytes in, bytes out: no OS calls, so tests/test_iconfile.c covers it on the host. amarchy-icons (DOS glue, later
 * rows) reads a file, calls li_parse + li_splice, and writes the result atomically.
 *
 * Layout parsed (big endian, workbench.h): DiskObject 78 bytes, OldDrawerData 56 bytes when do_DrawerData, Image 1
 * (20 byte header + planes) when GadgetRender, Image 2 when SelectRender, DefaultTool, ToolTypes, ToolWindow, then the
 * tail: 6 bytes of drawer data extension (drawers) and an optional OS 3.5 "FORM ICON" chunk.
 * li_splice replaces the images and the gadget fields bound to them; every other byte is copied from the source.
 * Not PutDiskObject, which would rewrite the whole object.
 *
 * Two things shadow a classic image on 3.2 and are therefore removed on APPLY and put back on RESTORE: the FORM ICON
 * tail and the NewIcon block of the tooltypes ("*** DON'T EDIT THE FOLLOWING LINES!! ***" and its IM1= / IM2= lines;
 * owner question Q1). Everything else keeps its bytes. */
#ifndef AMARCHY_ICONFILE_CORE_H
#define AMARCHY_ICONFILE_CORE_H

#include <stddef.h>
#include <stdint.h>

#define LI_MAXTT    256         /* tooltype entries one file may carry */

/* one image as stored: the 20 byte header fields and the planes (not copied: they point into the source buffer) */
typedef struct li_image {
    int16_t  left, top;
    uint16_t width, height, depth;
    uint8_t  planepick, planeonoff;
    uint32_t imagedata, next;   /* the raw ImageData and NextImage fields (stock files hold stale pointers there) */
    const uint8_t *data;        /* depth * height * ((width + 15) / 16) * 2 bytes */
    size_t   dlen;
} li_image;

typedef struct li_tt {          /* one tooltype string */
    size_t off;                 /* offset of its u32 length word in the source buffer */
    size_t len;                 /* the length word's value, the NUL included */
} li_tt;

typedef struct li_file {
    const uint8_t *buf;
    size_t   len;
    int      is_drawer;         /* do_DrawerData nonzero */
    uint32_t gad_render, gad_select;   /* raw GadgetRender and SelectRender */
    uint16_t gad_w, gad_h, gad_flags;
    int      nimg;              /* 0..2: images present */
    li_image img[2];
    size_t   img_off;           /* offset of image 1 (the byte after the drawer data) */
    size_t   img_end;           /* offset one past the last image */
    uint32_t tt_raw;            /* raw do_ToolTypes field */
    size_t   tt_off;            /* offset of the tooltype array (its u32 byte count), valid when ntt >= 0 */
    size_t   tt_end;            /* one past the array */
    int      ntt;               /* tooltype entries; -1 when the file has no array */
    li_tt    tt[LI_MAXTT];
    size_t   rest_off;          /* the first byte after the tooltype array: ToolWindow, extension, tail */
    size_t   form_off, form_len;/* the FORM ICON tail, 0 when absent */
} li_file;

/* Parses buf. 0 when it is a DiskObject the splicer can handle, else a negative LI_E_*. Never reads past len. */
#define LI_E_SHORT   (-1)
#define LI_E_MAGIC   (-2)
#define LI_E_LAYOUT  (-3)
#define LI_E_TOOMANY (-4)
int li_parse(const uint8_t *buf, size_t len, li_file *f);

/* Number of NewIcon tooltypes (the header line and the IM1= / IM2= lines) and the index of the first (-1 when none). */
int li_newicon_count(const li_file *f, int *first);

/* What to write. */
typedef struct li_edit {
    int      nimg;              /* 0..2 */
    li_image img[2];
    uint16_t gad_w, gad_h, gad_flags;
    uint32_t gad_render, gad_select;
    int      strip_newicon;     /* drop the NewIcon tooltypes of the source */
    int      drop_form;         /* drop the FORM ICON tail of the source */
    /* RESTORE: put ins_src's NewIcon tooltypes (in order) back at index ins_at of the result (clamped), and
     * append form_src's FORM tail (when form_src is set) */
    const li_file *ins_src;
    int      ins_at;
    const li_file *form_src;
    uint32_t tt_raw;            /* do_ToolTypes to write when the source had none and the result has some */
} li_edit;

/* An APPLY of normal (and selected, may be NULL) over src: gadget size from the normal image, GADGHIMAGE when selected
 * is given, raw render and select values kept from src when set, else 1. NewIcon block and FORM tail go. */
void li_edit_apply(li_edit *e, const li_file *src, const li_image *normal, const li_image *selected);

/* A RESTORE: the images, gadget fields, NewIcon lines and FORM tail of backup over cur (cur is the file as APPLY wrote
 * it, possibly with later edits of its other fields). */
void li_edit_restore(li_edit *e, const li_file *cur, const li_file *backup);

/* Writes the spliced file to out (cap bytes). Returns its length, 0 when cap is too small. */
size_t li_splice(const li_file *src, const li_edit *e, uint8_t *out, size_t cap);

/* 1 (tall pixel variant, 32x16) when the screen is at least twice as wide as high (640x256 hires non-laced), else 0
 * (square pixel variant, 32x32). */
int li_pick_variant(unsigned screen_w, unsigned screen_h);

/* lucide.dat (tools/gen_lucide_icons.py): header "LUCI", sorted name index, one record per icon. */
typedef struct li_dat {
    const uint8_t *buf;
    size_t   len;
    unsigned count, depth, variants;
    unsigned vw[4], vh[4];
    size_t   index_off;
} li_dat;

int li_dat_open(const uint8_t *buf, size_t len, li_dat *d);
/* The normal and selected image of `name` in variant v. 0 found, -1 not. The images point into the dat. */
int li_dat_find(const li_dat *d, const char *name, int v, li_image *normal, li_image *selected);

/* "Vol:Dir/Name.info" -> "SYS:Storage/Icons/Amarchy/Vol/Dir/Name.info". 0 ok, -1 when it does not fit, has no volume,
 * no ".info" suffix or a '/' before the colon. */
int li_backup_path(const char *icon_file, char *out, size_t cap);

#endif
