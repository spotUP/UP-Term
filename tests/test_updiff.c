#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "../install/updiff.h"

/* The Installer's update: which parts run again and what the copy parts copy
 * (installer update plan, U3). Manifests as tools/mkmanifest.py writes them. */

static char out[4096], want[4096];

static void collect(void *ctx, const char *part, const char *line)
{
    (void)ctx;
    strcat(out, part);
    strcat(out, ": ");
    strcat(out, line);
    strcat(out, "\n");
}

static const ud_part *find(const ud_result *r, const char *name)
{
    int i;
    for (i = 0; i < r->nparts; i++)
        if (!strcmp(r->part[i].name, name)) return &r->part[i];
    return 0;
}

static int runs(const ud_result *r, const char *name)
{
    const ud_part *p = find(r, name);
    return p && p->run;
}

static const char *const old_lines[] = {
    "tools 300 0000dddd Files/UPTerm\n",
    "copy-coreutils 100 0000aaaa Files/coreutils/bin/ls\n",
    "copy-coreutils 50 0000bbbb Files/coreutils/bin/sort\n",
    "copy-nvim 10 00000001 Files/nvim/share/a.vim\n",
    "system 2000 0000cccc Files/vsh\n",
    "- 7 00001234 README.txt\n",
    "copy-coreutils 0 00000000 copy#Files/coreutils/bin#bin\n",
    "copy-nvim 0 00000000 copy#Files/nvim#nvim\n",
    "all 0 11111111 install.dos#all\n",
    "copy-nvim 0 33333333 install.dos#copy-nvim\n",
    "system 0 22222222 install.dos#system\n",
    "tools 0 44444444 install.dos#tools\n",
    0
};

static void build(char *dst, const char *const *l)
{
    *dst = 0;
    for (; *l; l++) strcat(dst, *l);
}

/* nothing changed: nothing runs, nothing is copied */
static void same_kit(void)
{
    char a[2048], b[2048];
    ud_result r;
    build(a, old_lines);
    build(b, old_lines);
    out[0] = 0;
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), 0);
    CHECK_INT(r.files, 0);
    CHECK_INT(r.gone, 0);
    CHECK_INT((int)r.bytes, 0);
    CHECK_INT(runs(&r, "system") || runs(&r, "tools") || runs(&r, "copy-coreutils"), 0);
    CHECK_STR(out, "");
}

/* one changed file of a step part: that part runs, its bytes count */
static void one_changed_step_file(void)
{
    char a[2048], b[2048], *p;
    ud_result r;
    build(a, old_lines);
    build(b, old_lines);
    p = strstr(b, "2000 0000cccc Files/vsh");
    memcpy(p, "2100 0000ccce", 13);
    out[0] = 0;
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), 0);
    CHECK_INT(runs(&r, "system"), 1);
    CHECK_INT(runs(&r, "tools"), 0);
    CHECK_INT((int)find(&r, "system")->bytes, 2100);
    CHECK_INT((int)ud_kb(find(&r, "system")), 3);
    CHECK_INT(r.files, 1);
    CHECK_STR(out, "");
}

static const char *const new_lines[] = {
    "tools 300 0000dddd Files/UPTerm\n",
    "copy-coreutils 120 0000aaab Files/coreutils/bin/ls\n",
    "copy-coreutils 9 0000eeee Files/coreutils/bin/x/y/tr\n",
    "copy-nvim 10 00000001 Files/nvim/share/a.vim\n",
    "system 2000 0000cccc Files/vsh\n",
    "- 7 00001234 README.txt\n",
    "copy-coreutils 0 00000000 copy#Files/coreutils/bin#bin\n",
    "copy-nvim 0 00000000 copy#Files/nvim#nvim\n",
    "all 0 11111111 install.dos#all\n",
    "copy-nvim 0 33333333 install.dos#copy-nvim\n",
    "system 0 22222222 install.dos#system\n",
    "tools 0 44444444 install.dos#tools\n",
    0
};

static const char *const script_lines[] = {
    "copy-coreutils: Copy \"VTC:distkit/Files/coreutils/bin/ls\" \"UP-Term:bin/ls\" CLONE QUIET\n",
    "copy-coreutils: If EXISTS \"UP-Term:bin/sort\"\n",
    "copy-coreutils:   Delete \"UP-Term:bin/sort\" QUIET\n",
    "copy-coreutils: EndIf\n",
    "copy-coreutils: If NOT EXISTS \"UP-Term:bin/x\"\n",
    "copy-coreutils:   MakeDir \"UP-Term:bin/x\"\n",
    "copy-coreutils: EndIf\n",
    "copy-coreutils: If NOT EXISTS \"UP-Term:bin/x/y\"\n",
    "copy-coreutils:   MakeDir \"UP-Term:bin/x/y\"\n",
    "copy-coreutils: EndIf\n",
    "copy-coreutils: Copy \"VTC:distkit/Files/coreutils/bin/x/y/tr\" \"UP-Term:bin/x/y/tr\" CLONE QUIET\n",
    0
};

/* a copy part: the new file and the changed one copied, a new drawer made, the gone one deleted */
static void copy_part_script(void)
{
    char a[2048], b[2048];
    ud_result r;
    build(a, old_lines);
    build(b, new_lines);
    out[0] = 0;
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), 0);
    build(want, script_lines);
    CHECK_STR(out, want);
    CHECK_INT((int)find(&r, "copy-coreutils")->bytes, 129);
    CHECK_INT(r.files, 2);
    CHECK_INT(r.gone, 1);
    CHECK_INT(runs(&r, "copy-nvim"), 0);
    CHECK_INT(runs(&r, "system"), 0);
}

/* a part's script changed: it runs with nothing to copy; the head changed: every step part runs */
static void script_sections(void)
{
    char a[2048], b[2048], *p;
    ud_result r;
    build(a, old_lines);
    build(b, old_lines);
    p = strstr(b, "44444444 install.dos#tools");
    memcpy(p, "44444445", 8);
    CHECK_INT(ud_compare(a, b, "Work:UP-Term", &r, 0, 0), 0);
    CHECK_INT(runs(&r, "tools"), 1);
    CHECK_INT((int)ud_kb(find(&r, "tools")), 1);
    CHECK_INT(runs(&r, "system"), 0);
    build(a, old_lines);
    build(b, old_lines);
    p = strstr(b, "11111111 install.dos#all");
    memcpy(p, "11111112", 8);
    CHECK_INT(ud_compare(a, b, "Work:UP-Term", &r, 0, 0), 0);
    CHECK_INT(runs(&r, "tools") && runs(&r, "system"), 1);
    CHECK_INT(runs(&r, "copy-nvim"), 0);   /* the Installer copies those itself */
}

/* a step part's file gone: the part runs (its input set changed); "-" files never count */
static void gone_and_unowned(void)
{
    char a[2048], b[2048], *p, *e;
    ud_result r;
    build(a, old_lines);
    build(b, old_lines);
    p = strstr(b, "tools 300 0000dddd Files/UPTerm\n");
    e = strchr(p, '\n') + 1;
    memmove(p, e, strlen(e) + 1);
    p = strstr(b, "7 00001234 README.txt");
    memcpy(p, "8 00001235", 10);
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, 0, 0), 0);
    CHECK_INT(runs(&r, "tools"), 1);
    CHECK_INT(r.gone, 1);
    CHECK_INT(r.files, 0);
}

/* a manifest that is not one is refused, not half used */
static void bad_manifests(void)
{
    char a[2048], b[256];
    ud_result r;
    build(a, old_lines);
    strcpy(b, "system 12 zz Files/vsh\n");
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, 0, 0), -1);
    build(a, old_lines);
    strcpy(b, "tools 1 00000001 Files/b\nsystem 1 00000001 Files/a\n");
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, 0, 0), -1);   /* not sorted */
}

void suite_updiff(void)
{
    same_kit();
    one_changed_step_file();
    copy_part_script();
    script_sections();
    gone_and_unowned();
    bad_manifests();
}
