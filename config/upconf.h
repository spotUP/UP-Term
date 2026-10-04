/* upconf: the user configuration of an UP-Term window -- a plain-text file
 * of named profiles, parsed with no OS calls so it can be host-tested
 * (tests/test_upconf.c) and used by the XCON: handler and the Prefs app
 * without pulling in an Amiga.
 *
 * The file (ENV:up-term/up-term, saved in ENVARC:; plan
 * thoughts/shared/plans/2026-10-01-terminal-preferences.md):
 *
 *   ; comments with ; or #, blank lines free
 *   [profile default]
 *   font = TOPAZ 8.8.font      ; any text: the caller's own syntax
 *   fg = C0C0C0                ; RRGGBB hex
 *   scrollback = 2000
 *   cursor = bar               ; block | underline | bar
 *   bell = visual              ; none | beep | visual
 *
 *   [profile vim]
 *   font = PARADISEC 8.8.font
 *   bell = none
 *
 * Keys and profile names are case-insensitive; comment lines are kept and
 * written back where they stood (UC_MAX_NOTES); a later [profile] section of
 * the same name adds to the earlier one (last value wins). The caller decides
 * what a value means: upconf only stores and hands back text, so a new knob
 * needs no change here. A value longer than the caps is kept truncated, not
 * dropped, and counted in overflow so a caller can warn.
 *
 * A missing or empty file is not an error: every lookup then misses and the
 * caller's built-in defaults stand, which is how a window with no config
 * behaves exactly as before this feature.
 *
 * Portable C89: declarations at block start, no // comments, no stdint.h.
 * upconf is a fixed-size struct (no internal allocation): the caller provides
 * it on the stack or with its own allocator (the handler AllocVecs one).
 */
#ifndef UPCONF_H
#define UPCONF_H

/* Capacity: 8 profiles ("default" plus 7), 32 keys each, 160 bytes a value.
 * The value width is set by the palette, the longest single value the
 * format has: 16 remap entries as "II,RRGGBB," is ten bytes each, 159 with
 * the last comma dropped, which fills a 160-byte slot exactly. 64 held about
 * six of them, so a full grid could not be written. The struct is then
 * 8*32*160 = 41 KB, allocated once per session by the handler.
 * UC_MAX_FILE is the file both ends agree on: the handler refuses to read
 * more, the editor refuses to write more, so a file it writes is always a
 * file the handler can read whole. */
#define UC_MAX_PROFILES 8
#define UC_MAX_KEYS     32
#define UC_NAME         32
#define UC_MAX_VALUE    160
#define UC_MAX_FILE     16384

/* Comment lines (; or #) are kept too, so a save writes back what it read:
 * each one as typed, anchored to the key it stood before (or after the
 * profile's last key, or before every section). The shipped sample is 45
 * lines, 2.9 KB; the room is twice that. Blank lines are not kept: the
 * save puts one between profiles. More comment text than this marks
 * overflow, so the editor will not write such a file back. */
#define UC_MAX_NOTES    160
#define UC_NOTE_BYTES   6144

typedef unsigned long uc_u32;

typedef struct upconf {
    char prof[UC_MAX_PROFILES][UC_NAME];
    char key[UC_MAX_PROFILES][UC_MAX_KEYS][UC_NAME];
    char val[UC_MAX_PROFILES][UC_MAX_KEYS][UC_MAX_VALUE];
    int  n[UC_MAX_PROFILES];    /* keys in use in this profile */
    int  nprof;                 /* profiles in use */
    int  overflow;              /* value truncated or table full: set, not fatal */
    /* the comments, in file order: note_at[i] is where line i starts in
     * note (NUL-terminated), note_prof[i] its profile (-1: before every
     * section), note_key[i] the key it stands before (n[]: after the last) */
    char note[UC_NOTE_BYTES];
    short note_at[UC_MAX_NOTES];
    signed char note_prof[UC_MAX_NOTES];
    unsigned char note_key[UC_MAX_NOTES];
    int  nnote;                 /* comment lines kept */
    int  notelen;               /* bytes of note in use */
} upconf;

/* Parse a whole NUL-terminated file. 0 when buf is NULL (the conf is then
 * left empty and every lookup misses: defaults stand), 1 otherwise. What does
 * not fit the caps is kept truncated; overflow marks it. */
int  upconf_parse(upconf *c, const char *buf, long len);
/* Does profile hold the same keys with the same values in a and b (order
 * and key case aside; a profile absent from both is equal)? A window's
 * live update: only a window whose own profile changed is redrawn. */
int  upconf_profile_equal(const upconf *a, const upconf *b, const char *profile);
/* The last value of key in profile, 0 when absent. Case-insensitive. */
const char *upconf_get(const upconf *c, const char *profile, const char *key);
/* upconf_get, or def when the key is absent or its value is empty. */
const char *upconf_str(const upconf *c, const char *profile, const char *key, const char *def);
/* upconf_str as a number; def when absent, empty or not a number.
 * A negative value is returned as is (scrollback -1 = none). */
long upconf_int(const upconf *c, const char *profile, const char *key, long def);
/* A hex number, 0xRRGGBB: def when absent, empty or not hex. Accepts
 * rrggbb with or without a leading # or 0x; trailing junk stops the number. */
uc_u32 upconf_hex(const char *v, uc_u32 def);
/* Exactly six hex digits and nothing else (leading # or 0x and trailing
 * spaces allowed); 0 with *rgb untouched otherwise. */
int  upconf_hex6(const char *s, uc_u32 *rgb);
/* upconf_str as 0xRRGGBB; def when the key is absent or its value is not hex. */
uc_u32 upconf_rgb(const upconf *c, const char *profile, const char *key, uc_u32 def);
/* Parse a "palette" value, "index,RRGGBB,index,RRGGBB,...": out16 (room for
 * 16) is zeroed first, each valid entry becomes 0x01RRGGBB and the rest stay
 * 0 (not remapped). The index is decimal 0-15, the colour hex with an
 * optional 0x or # prefix; a malformed pair stops the list. The entries set. */
int  upconf_palette_parse(const char *s, uc_u32 *out16);
/* The inverse: the 0x01RRGGBB entries of in16 (16) as "index,RRGGBB,...".
 * The length written (without a terminating NUL), or -1 when it does not fit;
 * the buffer is not NUL-terminated, so the caller keeps room for the NUL. */
long upconf_palette_str(const uc_u32 *in16, char *out, long cap);
/* 1 when the key is present at all (an empty value counts). */
int  upconf_has(const upconf *c, const char *profile, const char *key);
/* The profile names in the order set, into the caller's array of room for
 * UC_MAX_PROFILES + 1 (the last is ""). Returns the count (0 when none). */
int  upconf_profiles(const upconf *c, const char **names);

/* Set a value: adds the profile and the key when new, replaces when the key
 * exists. 1 on success, 0 when the table is full (overflow marked). Used by
 * the Prefs app and the tests; the parser fills the same table. */
int  upconf_set(upconf *c, const char *profile, const char *key, const char *value);
/* Delete a key; the profile's other keys keep their order and the comments
 * their place. 1 when it was there. */
int  upconf_del(upconf *c, const char *profile, const char *key);
/* Delete a whole profile by name (case-insensitive), its comments with it;
 * the later profiles keep their order. 1 when it was there. */
int  upconf_rmprof(upconf *c, const char *profile);
/* Empty the table. */
void upconf_clear(upconf *c);

/* Write the whole table back, in the format the parser reads: the comments
 * before every section first, then every profile as a "[profile <name>]"
 * line, its keys in the order set as "key = value" lines with its comments
 * where they stood, a blank line between profiles. Values are left bare: the parser
 * reads to end-of-line, so no quoting is needed. Returns the length written
 * (without the terminating NUL), or -1 when it does not fit. */
long upconf_save(const upconf *c, char *buf, long cap);

#endif
