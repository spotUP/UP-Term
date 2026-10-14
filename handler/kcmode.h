/* KingCON's FNCMODE letters (W window, L list, B cycle, C common prefix
 * first, S silent; any case, other characters ignored) as LE_KC_* bits.
 * W clears L and B, as in KingCON. 0 letters: W.
 *
 * Portable C with no other dependency, so the line editor (handler) and
 * UP-Term Prefs (prefs_kc_decode) read the letters through the one function. */
#ifndef KCMODE_H
#define KCMODE_H

#define LE_KC_WINDOW 1
#define LE_KC_LIST   2
#define LE_KC_CYCLE  4
#define LE_KC_COMMON 8
#define LE_KC_SILENT 16
int  le_kc_fncmode(const char *letters);

#endif
