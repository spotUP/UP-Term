/* vtwidth.h -- how many cells a code point takes on a vtcon terminal.
 * One definition for the engine and for programs that lay text out for
 * it (tmux on AmigaOS: its idea of a width must be the terminal's, or its
 * panes drift from what XCON draws). Header-only so a port compiles it
 * without the engine; needs vt_u32 (vtengine.h) or a typedef of its own. */
#ifndef VTWIDTH_H
#define VTWIDTH_H

/* Display width: 0 for combining marks, 2 for East Asian wide and
 * fullwidth, 1 otherwise. Ranges as in Markus Kuhn's wcwidth, reduced to
 * the BMP blocks a terminal meets. */
static int vt_char_width(vt_u32 c)
{
    if (c < 0x300)
        return 1;
    if ((c >= 0x0300 && c <= 0x036F) || (c >= 0x0483 && c <= 0x0489) ||
        (c >= 0x0591 && c <= 0x05BD) || (c >= 0x0610 && c <= 0x061A) ||
        (c >= 0x064B && c <= 0x065F) || (c >= 0x0E31 && c <= 0x0E3A && c != 0x0E32 && c != 0x0E33) ||
        (c >= 0x1AB0 && c <= 0x1AFF) || (c >= 0x1DC0 && c <= 0x1DFF) ||
        (c >= 0x200B && c <= 0x200F) || (c >= 0x20D0 && c <= 0x20FF) ||
        (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xFE20 && c <= 0xFE2F) || c == 0xFEFF)
        return 0;
    if ((c >= 0x1100 && c <= 0x115F) || c == 0x2329 || c == 0x232A ||
        (c >= 0x2E80 && c <= 0xA4CF && c != 0x303F) || (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE4F) ||
        (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6))
        return 2;
    return 1;
}

#endif
