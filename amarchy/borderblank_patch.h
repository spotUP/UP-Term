/* AM-BORDERBLANK: the commodity blanks the display border (black outside the picture, not colour 0) on every open
 * native screen at install and patches intuition OpenScreenTagList (LVO -612) and OpenScreen (LVO -198) so each new
 * native screen gets it too. Remove clears the screens Amarchy blanked (never one its own program blanked) and
 * unpatches. RTG screens are left alone (the card draws no chipset border). Amiga only. */
#ifndef AMARCHY_BORDERBLANK_PATCH_H
#define AMARCHY_BORDERBLANK_PATCH_H

#define BB_SEM_NAME "Amarchy.BorderBlank"
#define BB_LVO_OPENSCREEN (-198)
#define BB_LVO_OPENSCREENTAGLIST (-612)

/* 0 = installed (or already installed by this task); negative = no patch, the screens keep the stock border. */
int borderblank_install(void);
void borderblank_remove(void);
/* 1 while installed. */
int borderblank_on(void);

#endif
