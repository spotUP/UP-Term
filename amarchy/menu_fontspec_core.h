/* The font spec the Amarchy drawn menu opens (TE-7b.1). Pure: no AmigaOS calls, so a host test links it. */
#ifndef MENU_FONTSPEC_CORE_H
#define MENU_FONTSPEC_CORE_H

#include "gthemes_core.h"

/* Writes into out (cap outsize) the "name:size" spec mt_open takes (prefs_font_parse: the name with or without ".font", ":size" optional).
 * Precedence, the same as the Icon Font and Screen Font: the active theme's Menu Font (look.font[GT_FONT_MENU], "name.font/size") when the theme
 * names one, else am_font (the user's AM_FONT pref), else "" (the built-in font). A theme font whose size is 0 or missing gives the bare name,
 * which mt_open reads as size 0 (the font's default size). A spec that does not fit out is cut at outsize - 1 and stays terminated;
 * t and am_font may be 0. Returns 1 when the theme's Menu Font was used, 0 otherwise. */
int mt_font_spec(const gtheme *t, const char *am_font, char *out, int outsize);

/* The drawn menu's entry: loads theme with gt_load_model (not gt_load: gt_load attaches no model, and the Menu Font lives in the model's look
 * block, so mt_font_spec would always fall back to am_font), runs mt_font_spec, frees the theme. Same return and out as mt_font_spec. */
int mt_font_spec_load(gt_read_fn rd, void *ctx, const char *theme, const char *am_font, char *out, int outsize);

#endif
