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

/* The kit split (plan D7): the new base's manifest has no Neovim files but one
 * handed# line for the path. */
static const char *const base_after_split[] = {
    "tools 300 0000dddd Files/UPTerm\n",
    "copy-coreutils 100 0000aaaa Files/coreutils/bin/ls\n",
    "copy-coreutils 50 0000bbbb Files/coreutils/bin/sort\n",
    "system 2000 0000cccc Files/vsh\n",
    "- 7 00001234 README.txt\n",
    "copy-coreutils 0 00000000 copy#Files/coreutils/bin#bin\n",
    "handed 0 00000000 handed#Files/nvim\n",
    "all 0 11111111 install.dos#all\n",
    "system 0 22222222 install.dos#system\n",
    "tools 0 44444444 install.dos#tools\n",
    0
};

/* a base update over the single kit: Neovim's files are the add-on's now, so the old copies are
 * not deleted, not counted, and the copy part and the handed archive are named */
static void base_update_over_the_single_kit_deletes_none_of_nvims_files(void)
{
    char a[2048], b[2048];
    ud_result r;
    build(a, old_lines);
    build(b, base_after_split);
    out[0] = 0;
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), 0);
    CHECK_STR(out, "");
    CHECK_INT(r.gone, 0);
    CHECK_INT(runs(&r, "copy-nvim"), 0);
    CHECK_INT(find(&r, "handed") != 0, 0);   /* the handed# line is no part */
    CHECK_INT(r.nhanded, 1);
    CHECK_STR(r.handed[0], "Files/nvim");
}

/* The 1.0 kit's manifest with its 9 handed# lines (and n of them in all, sorted):
 * the owner saw "not a kit manifest" and a full install instead of the Update
 * page, because the parser took 8 at most. */
static void base_with_handed(char *dst, int n)
{
    static const char *const kit[] = {
        "Files/Claude", "Files/amiga-pi", "Files/nvim", "Files/python3",
        "Files/userland/bin/git", "Files/userland/bin/perl", "Files/userland/bin/vim",
        "Files/userland/share/git-core", "Files/userland/share/vim"
    };
    char line[128];
    int i;
    *dst = 0;
    for (i = 0; base_after_split[i]; i++) {
        if (!strncmp(base_after_split[i], "handed ", 7)) {
            int k;
            for (k = 0; k < n; k++) {
                strcpy(line, "handed 0 00000000 handed#");
                if (k < 9) strcat(line, kit[k]);
                else {
                    char x[] = "Files/x00";
                    x[7] = (char)('0' + k / 10);
                    x[8] = (char)('0' + k % 10);
                    strcat(line, x);
                }
                strcat(line, "\n");
                strcat(dst, line);
            }
            continue;
        }
        strcat(dst, base_after_split[i]);
    }
}

static void an_update_over_the_kit_with_nine_handed_paths_is_offered(void)
{
    static char a[8192], b[8192];
    ud_result r;
    base_with_handed(a, 9);
    base_with_handed(b, 9);
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), 0);
    CHECK_INT(r.files, 0);
    CHECK_INT(r.gone, 0);
    /* the old kit had the add-on's files: every handed path is named, none dropped */
    build(a, old_lines);
    base_with_handed(b, 9);
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), 0);
    CHECK_INT(r.nhanded, 1);   /* only Files/nvim was in the old kit */
    CHECK_STR(r.handed[0], "Files/nvim");
    /* more handed lines than the parser holds is still no kit manifest, not an overrun */
    base_with_handed(a, UD_MAXHANDED + 1);
    base_with_handed(b, UD_MAXHANDED + 1);
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), -1);
}

/* the handed files are not in the KB to copy; the base's own changed file is */
static void a_handed_file_is_not_counted_in_the_kb_to_copy(void)
{
    char a[2048], b[2048], *p;
    ud_result r;
    build(a, old_lines);
    build(b, base_after_split);
    p = strstr(b, "2000 0000cccc Files/vsh");
    memcpy(p, "2100 0000ccce", 13);
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, 0, 0), 0);
    CHECK_INT((int)r.bytes, 2100);
    CHECK_INT(r.files, 1);
    CHECK_INT(r.gone, 0);
    CHECK_INT(runs(&r, "copy-nvim"), 0);
    CHECK_INT(runs(&r, "system"), 1);
}

/* a file gone from a part that is not handed is still deleted, with the handed path beside it */
static void a_file_gone_from_a_copy_part_that_is_not_handed_is_still_deleted(void)
{
    char a[2048], b[2048], *p, *e;
    ud_result r;
    build(a, old_lines);
    build(b, base_after_split);
    p = strstr(b, "copy-coreutils 50 0000bbbb Files/coreutils/bin/sort\n");
    e = strchr(p, '\n') + 1;
    memmove(p, e, strlen(e) + 1);
    out[0] = 0;
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), 0);
    CHECK_STR(out, "copy-coreutils: If EXISTS \"UP-Term:bin/sort\"\ncopy-coreutils:   Delete \"UP-Term:bin/sort\" QUIET\n"
                   "copy-coreutils: EndIf\n");
    CHECK_INT(r.gone, 1);
    CHECK_INT(r.nhanded, 1);
}

/* ONLY keeps an add-on's lines of the single kit's manifest: its files and its copy# line */
static void only_keeps_the_add_ons_lines_and_drops_the_bases(void)
{
    char a[2048];
    build(a, old_lines);
    CHECK_INT(ud_only(a, "Files/nvim"), 2);
    CHECK_STR(a, "copy-nvim 10 00000001 Files/nvim/share/a.vim\ncopy-nvim 0 00000000 copy#Files/nvim#nvim\n");
    /* several paths, a file path, one path that only starts like another, and a space in a path */
    strcpy(a, "tools 5 00000001 Files/Claude\ntools 6 00000002 Files/ClaudeCode\ntools 7 00000003 Files/UP-Term Prefs\n"
              "tools 0 00000000 install.dos#tools\nhanded 0 00000000 handed#Files/Claude\n");
    CHECK_INT(ud_only(a, "Files/Claude,Files/UP-Term Prefs"), 2);
    CHECK_STR(a, "tools 5 00000001 Files/Claude\ntools 7 00000003 Files/UP-Term Prefs\n");
    strcpy(a, "system 1 00000001 Files/vsh\n");
    CHECK_INT(ud_only(a, "Files/nvim"), 0);
    CHECK_STR(a, "");
}

/* the single kit's nvim lines, cut by ONLY, against an add-on's manifest: nothing to copy */
static void only_then_compare_copies_only_what_changed(void)
{
    static const char *const nvim_add_on[] = {
        "copy-nvim 10 00000001 Files/nvim/share/a.vim\n",
        "copy-nvim 20 00000002 Files/nvim/share/b.vim\n",
        "copy-nvim 0 00000000 copy#Files/nvim#nvim\n",
        "copy-nvim 0 33333333 install.dos#copy-nvim\n",
        0
    };
    char a[2048], b[2048];
    ud_result r;
    build(a, old_lines);
    ud_only(a, "Files/nvim");
    build(b, nvim_add_on);
    out[0] = 0;
    CHECK_INT(ud_compare(a, b, "Work:UP-Term-nvim", &r, collect, 0), 0);
    CHECK_INT(r.files, 1);   /* only b.vim is new */
    CHECK_INT(r.gone, 0);
    CHECK_STR(out, "copy-nvim: If NOT EXISTS \"UP-Term:nvim/share\"\ncopy-nvim:   MakeDir \"UP-Term:nvim/share\"\ncopy-nvim: EndIf\n"
                   "copy-nvim: Copy \"Work:UP-Term-nvim/Files/nvim/share/b.vim\" \"UP-Term:nvim/share/b.vim\" CLONE QUIET\n");
}

/* a file whose part was renamed (tools to claude), same size and CRC, is not copied again */
static void a_file_whose_part_was_renamed_is_not_copied(void)
{
    char a[2048], b[2048];
    ud_result r;
    strcpy(a, "tools 300 0000dddd Files/Claude\ntools 0 44444444 install.dos#tools\n");
    strcpy(b, "claude 300 0000dddd Files/Claude\nclaude 0 55555555 install.dos#claude\n");
    out[0] = 0;
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), 0);
    CHECK_INT(r.files, 0);
    CHECK_INT((int)r.bytes, 0);
    /* the new part's own script still runs (it is new), and the old part's script line is gone: no file */
    CHECK_INT(runs(&r, "claude"), 1);
    /* a changed file under the new part name is copied */
    strcpy(a, "tools 300 0000dddd Files/Claude\ntools 0 44444444 install.dos#tools\n");
    strcpy(b, "claude 301 0000dddd Files/Claude\nclaude 0 55555555 install.dos#claude\n");
    CHECK_INT(ud_compare(a, b, "VTC:distkit", &r, collect, 0), 0);
    CHECK_INT(r.files, 1);
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
    base_update_over_the_single_kit_deletes_none_of_nvims_files();
    an_update_over_the_kit_with_nine_handed_paths_is_offered();
    a_handed_file_is_not_counted_in_the_kb_to_copy();
    a_file_gone_from_a_copy_part_that_is_not_handed_is_still_deleted();
    only_keeps_the_add_ons_lines_and_drops_the_bases();
    only_then_compare_copies_only_what_changed();
    a_file_whose_part_was_renamed_is_not_copied();
}
