/* winspec: the XCON: window spec's size fields, resolved against the screen,
 * with no OS calls so the host tests cover it (tests/test_winspec.c).
 *
 * A width or height of 0 in "XCON:x/y/w/h/..." means "as wide / as high as
 * the screen, from the left / top edge on" (the Workbench icon's window).
 * Any other value is the size as given. Portable C89.
 */
#ifndef WINSPEC_H
#define WINSPEC_H

/* want: the spec's width or height; origin: its left or top edge; extent: the
 * screen's width or height. Returns the size to open the window with. */
int winspec_size(int want, int origin, int extent);

#endif
