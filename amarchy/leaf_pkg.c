#include <stdio.h>
#include <string.h>
#include "leaf.h"

/* The Packages group glue that needs the index (ix_pkg_*). Kept out of leaf_core.c so programs that only need its
 * shell/launch/setting helpers (jobprobe, wbrun) do not drag the index and menu modules in. */

int lf_pkg_search_row(const char *node, const mn_item *it)
{
    return node && it && MN_IS_GROUP(it) && !strcmp(node, IX_PKGS_NODE) && !strcmp(it->seg, "search");
}

int lf_pkg_search(ix_index *x, const lf_ui *ui)
{
    char text[64];
    text[0] = 0;
    if (!x || !ui || !ui->ask_value || !ui->ask_value(ui->ctx, "Search packages", "text", text, sizeof text) || !text[0])
        return 0;
    ix_pkg_query(x, text);
    return 1;
}

int lf_pkg_back_levels(const mn_item *it)
{
    if (!it || !MN_IS_PKGACT(it) || it->ix < 0)
        return 0;
    return it->seg[0] == PK_ACT_REMOVE ? 2 : it->seg[0] == PK_ACT_NO || it->seg[0] == PK_ACT_INSTALL ||
           it->seg[0] == PK_ACT_UPGRADE ? 1 : 0;
}

void lf_job_done(ix_index *x, const char *line, int rc)
{
    (void)rc;                           /* the exit code does not matter: even a failed job may have changed packages */
    if (x && line && !strncmp(line, "upkg ", 5))
        ix_pkg_mark_stale(x);
}
