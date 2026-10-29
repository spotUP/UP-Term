/* gthemes_dos: see gthemes_dos.h. */
#include <string.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "gthemes_dos.h"
#include "amconf_dos.h"
#include "../prefs/prefs_dos.h"
#include "menu_theme.h"
#include "rexx_host.h"

unsigned long gt_dos_dopus_sends;

static long gt_dopus_send(const char *port, const char *line, void *ud)
{
    (void)ud;
    return rexx_send(port, line, 0, 0);
}

static long gt_read(void *ctx, const char *path, char *buf, long cap)
{
    BPTR f = Open((STRPTR)path, MODE_OLDFILE);
    LONG got;
    (void)ctx;
    if (!f)
        return -1;
    got = Read(f, buf, cap);
    Close(f);
    return got < 0 ? 0 : got;
}

void gt_dos_load(const char *name, gtheme *out)
{
    gt_load(gt_read, 0, name, out);
}

void gt_dos_current(char *out, int cap)
{
    am_prefs ap;
    const char *v;
    amconf_load(&ap);
    v = ap.val[AM_GTHEME];
    if (!*v || (int)strlen(v) >= cap)
        v = GT_DEFAULT;
    strcpy(out, v);
}

void gt_dos_load_current(gtheme *out)
{
    char name[GT_NAME];
    gt_dos_current(name, sizeof name);
    gt_dos_load(name, out);
}

int gt_dos_select(const char *name, char *why, int cap)
{
    am_prefs ap;
    int failed = 0, r;
    if (!gt_name_ok(name)) {
        if (why)
            gt_save_text(GT_SAVE_NAME, 0, 0, why, cap);
        return 0;
    }
    amconf_load(&ap);
    strcpy(ap.val[AM_GTHEME], name);
    r = amconf_save(&ap, 1, &failed);
    if (r == PREFS_INSTALL_OK) {
        gtheme th;
        gt_dos_load(name, &th);
        gt_dos_dopus_sends++;
        dopus_theme_send(&th, gt_dopus_send, 0);
        return 1;
    }
    if (why)
        gt_save_text(r, failed, prefs_dos_last_error(), why, cap);
    return 0;
}

void gt_dos_load_rows(ix_index *x)
{
    static gt_list l;
    static char lab[GT_MAX][GT_NAME + 16];
    const char *labels[GT_MAX], *names[GT_MAX];
    char cur[GT_NAME];
    int i;
    gt_dos_scan(&l);
    gt_dos_current(cur, sizeof cur);
    for (i = 0; i < l.n; i++) {
        gt_label(l.name[i], cur, lab[i], (int)sizeof lab[i]);
        labels[i] = lab[i];
        names[i] = l.name[i];
    }
    ix_add_gthemes(x, labels, names, l.n);
}
