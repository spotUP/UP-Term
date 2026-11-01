/* The console's history file (handler/histfile.h): out of ENVARC: into the
 * kit's UP-Term:var/vtcon by claude/datadir's rule, an old file moved on
 * first use, ENVARC:vtcon.history without the kit, and a shell's own
 * HISTFILE whatever the default is. ENVARC: and UP-Term: are host
 * directories (tests/claude_vol.c). */
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "harness.h"
#include "claude_vol.h"
#include "../claude/sys_posix.h"
#include "../handler/histfile.h"
#include "../claude/datadir.h"
#include "../claude/hist.h"

static sys_posix sp;
static cl_sys inner, vsys;
static cl_vol vol;

static void volumes(const char *dir, int kit, int no_rename)
{
    memset(&vol, 0, sizeof(vol));
    sys_posix_init(&sp, &inner);
    sprintf(vol.envarc, "%s/envarc", dir);
    mkdir(vol.envarc, 0700);
    if (kit) {
        sprintf(vol.kit, "%s/kit", dir);
        mkdir(vol.kit, 0700);
    }
    vol.no_rename = no_rename;
    vol_init(&vol, &inner, &vsys);
}

static void done(const char *dir)
{
    dd_remove_tree(&inner, dir);
}

static char *text(const char *path)
{
    char *b = 0;
    long n = 0;
    return vsys.read(vsys.u, path, 1L << 20, &b, &n) ? 0 : b;
}

static int fresh_dir(char *dir, long cap)
{
    const char *t = getenv("TMPDIR");
    if ((long)strlen(t && *t ? t : "/tmp") + 20 > cap)
        return 0;
    sprintf(dir, "%s/histfile_XXXXXX", t && *t ? t : "/tmp");
    return mkdtemp(dir) != 0;
}

/* with the kit, an older UP-Term's ENVARC:vtcon.history moves to
 * UP-Term:var/vtcon/history (copied across volumes) and is used from there */
static void history_moves_out_of_envarc_into_the_kit_drawer(void)
{
    char dir[400], out[300], *b;
    if (!fresh_dir(dir, sizeof(dir)))
        return;
    volumes(dir, 1, 1);
    vsys.write(vsys.u, HF_OLD, "ls\npwd\n", 7);
    CHECK_INT(hf_path(&vsys, 0, out, sizeof(out)), 0);
    CHECK_STR(out, "UP-Term:var/vtcon/history");
    b = text("UP-Term:var/vtcon/history");
    CHECK_STR(b ? b : "(none)", "ls\npwd\n");
    free(b);
    CHECK_INT(vsys.kind(vsys.u, HF_OLD), 0);
    CHECK(vol.quiet_kinds > 0);        /* UP-Term: asked without a requester */
    /* the next window: nothing left to move, the same file */
    CHECK_INT(hf_path(&vsys, "", out, sizeof(out)), 0);
    CHECK_STR(out, "UP-Term:var/vtcon/history");
    done(dir);
}

/* a fresh kit with no old file: the drawer is made, so the first line can be appended */
static void kit_without_an_old_file_gets_its_drawer(void)
{
    char dir[400], out[300];
    if (!fresh_dir(dir, sizeof(dir)))
        return;
    volumes(dir, 1, 0);
    CHECK_INT(hf_path(&vsys, 0, out, sizeof(out)), 0);
    CHECK_STR(out, "UP-Term:var/vtcon/history");
    CHECK_INT(vsys.kind(vsys.u, "UP-Term:var/vtcon"), 2);
    done(dir);
}

/* without the kit: ENVARC:vtcon.history, untouched */
static void without_the_kit_the_history_stays_in_envarc(void)
{
    char dir[400], out[300], *b;
    if (!fresh_dir(dir, sizeof(dir)))
        return;
    volumes(dir, 0, 0);
    vsys.write(vsys.u, HF_OLD, "old\n", 4);
    CHECK_INT(hf_path(&vsys, 0, out, sizeof(out)), 0);
    CHECK_STR(out, HF_OLD);
    b = text(HF_OLD);
    CHECK_STR(b ? b : "(none)", "old\n");
    free(b);
    done(dir);
}

/* a shell's HISTFILE is its file, kit or not, and nothing is moved for it */
static void shell_histfile_is_used_as_given(void)
{
    char dir[400], out[300];
    if (!fresh_dir(dir, sizeof(dir)))
        return;
    volumes(dir, 1, 0);
    vsys.write(vsys.u, HF_OLD, "old\n", 4);
    CHECK_INT(hf_path(&vsys, "RAM:rig.history", out, sizeof(out)), 0);
    CHECK_STR(out, "RAM:rig.history");
    CHECK_INT(vsys.kind(vsys.u, HF_OLD), 1);
    CHECK_INT(hf_path(&vsys, "RAM:rig.history", out, 4), -1);
    done(dir);
}

/* A name "path\nproject" is a file of the prompt history's JSON lines (what amiga-pi's raw mode keeps): the handler
 * loads the prompts of that project as its plain lines and appends in the same form, so line mode and raw mode
 * recall one list. A plain name stays the Shell's plain lines. */
static void prompt_history_file_is_shared_with_raw_mode(void)
{
    char dir[400], out[300], file[400], spec[420], *b, *data;
    cl_hist h;
    long n, m;
    if (!fresh_dir(dir, sizeof(dir)))
        return;
    volumes(dir, 1, 0);
    sprintf(file, "%s/prompt-history", dir);
    hist_init(&h);
    CHECK_INT(hist_add(&h, &inner, file, "first \"prompt\"", ""), 0);
    CHECK_INT(hist_add(&h, &inner, file, "other project", "/x"), 0);
    hist_free(&h);
    sprintf(spec, "%s\n", file);
    CHECK_INT(hf_path(&vsys, spec, out, sizeof(out)), 0);
    CHECK_STR(out, file);
    CHECK_INT(inner.read(inner.u, out, 1L << 20, &b, &n), 0);
    data = b;
    m = hf_load(data, n, spec);
    data[m] = 0;
    CHECK_STR(data, "first \"prompt\"\n");
    free(b);
    /* a line entered in line mode is appended as a JSON line raw mode loads */
    CHECK_INT(hf_line(spec, "typed in line mode", out, sizeof(out)) > 0, 1);
    {
        FILE *f = fopen(file, "ab");
        if (f) {
            fputs(out, f);
            fclose(f);
        }
    }
    hist_init(&h);
    CHECK_INT(hist_load(&h, &inner, file), 3);
    CHECK_INT(h.n, 3);
    CHECK_STR(h.n == 3 ? h.d[2] : "", "typed in line mode");
    CHECK_STR(h.n == 3 ? h.p[2] : "x", "");
    hist_free(&h);
    /* a plain name stays plain */
    CHECK_INT(hf_line("RAM:h", "ls", out, sizeof(out)), 3);
    CHECK_STR(out, "ls\n");
    done(dir);
}

void suite_histfile(void)
{
    history_moves_out_of_envarc_into_the_kit_drawer();
    kit_without_an_old_file_gets_its_drawer();
    without_the_kit_the_history_stays_in_envarc();
    shell_histfile_is_used_as_given();
    prompt_history_file_is_shared_with_raw_mode();
}
