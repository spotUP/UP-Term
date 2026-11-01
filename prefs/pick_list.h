/* pick_list: a small window with a list to choose one entry from (OK / Cancel, or a double-click). Upprefs uses it for
 * the Menu theme; the font and profile rows reuse it. The decisions (which entry opens selected, how far the list is
 * scrolled, what the window returns) are plain C here, host-tested; the window itself is pick_list_gt.c. */
#ifndef PICK_LIST_H
#define PICK_LIST_H

#define PK_ROWS 10      /* most rows the list shows; a longer list scrolls */

/* the entry the list opens with selected: current when it is one of the n entries, else 0 */
int pk_initial(int n, int current);
/* the rows the list shows for n entries: n, at most PK_ROWS, at least 1 */
int pk_rows(int n);
/* the first row shown so entry sel is in view, in the middle when the list scrolls (0..n-rows) */
int pk_top(int n, int rows, int sel);
/* what the window returns: sel when confirmed (OK, double-click) and a real entry, else -1 (Cancel, close, Esc) */
int pk_result(int n, int sel, int confirmed);

struct Screen;
struct Window;
struct TextAttr;

/* Opens the picker over parent (blocked while it is open) on scr: text[0..n-1] are the entries, sel the one selected
 * to begin with, vi the screen's GadTools visual info, ta its font, fw / fh the font's cell size (every size is
 * computed from them). Returns the entry chosen, or -1 for Cancel / the close gadget / Esc / no memory. */
int pk_run(struct Screen *scr, struct Window *parent, void *vi, struct TextAttr *ta, int fw, int fh,
           const char *title, const char *const *text, int n, int sel);

#endif
