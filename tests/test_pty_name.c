/* PTY: names and open rules (handler/pty_name.h): a pipe's ends parse and
 * open, so vsh's pipes (`coproc cat`, pipelines) get a Read that answers
 * with what the pipe holds instead of PIPE:'s whole-buffer Read. */
#include "harness.h"
#include "../handler/pty_name.h"
#include <string.h>

static int parse(const char *s, char *id)
{
    unsigned char b[64];
    b[0] = (unsigned char)strlen(s);
    memcpy(b + 1, s, b[0]);
    id[0] = 0;
    return pn_parse(b, id);
}

/* vsh opens PTY:v<task><n>/w then /r: both are a pipe's ends under one id */
static void pipe_ends_parse_as_a_pipe(void)
{
    char id[PN_ID_MAX];
    CHECK_INT(parse("PTY:v0812abcd1f/w", id), PN_PIPE_W);
    CHECK(!strcmp(id, "v0812abcd1f"));
    CHECK_INT(parse("PTY:v0812abcd1f/r", id), PN_PIPE_R);
    CHECK(!strcmp(id, "v0812abcd1f"));
    CHECK_INT(parse("pty:x/W", id), PN_PIPE_W);
    CHECK_INT(parse("PTY:x/R", id), PN_PIPE_R);
}

/* the terminal names keep their meaning, and bad names stay bad */
static void terminal_names_and_bad_names_are_unchanged(void)
{
    char id[PN_ID_MAX];
    CHECK_INT(parse("PTY:7/m", id), PN_MASTER);
    CHECK(!strcmp(id, "7"));
    CHECK_INT(parse("PTY:7/s", id), PN_SLAVE);
    CHECK_INT(parse("*", id), PN_PORT);
    CHECK_INT(parse("console:", id), PN_PORT);
    CHECK_INT(parse("PTY:7/x", id), PN_BAD);
    CHECK_INT(parse("PTY:7/ww", id), PN_BAD);
    CHECK_INT(parse("PTY:/w", id), PN_BAD);
    CHECK_INT(parse("PTY:0123456789abcdef/w", id), PN_BAD); /* 16 characters */
    CHECK_INT(parse("PTY:7", id), PN_BAD);
}

/* MODE_NEWFILE on /w makes the pipe; a taken id is refused so the opener
 * tries the next one; /r and an OLDFILE /w join an existing pipe only */
static void pipe_open_rules(void)
{
    CHECK_INT(pn_open_rule(PN_PIPE_W, 0, 0, 0, 1), PN_NEW);
    CHECK_INT(pn_open_rule(PN_PIPE_W, 1, 1, 0, 1), PN_IN_USE);
    CHECK_INT(pn_open_rule(PN_PIPE_W, 1, 0, 0, 1), PN_IN_USE); /* a terminal's id */
    CHECK_INT(pn_open_rule(PN_PIPE_W, 1, 1, 0, 0), PN_OPEN);
    CHECK_INT(pn_open_rule(PN_PIPE_W, 0, 0, 0, 0), PN_NOT_FOUND);
    CHECK_INT(pn_open_rule(PN_PIPE_R, 1, 1, 0, 0), PN_OPEN);
    CHECK_INT(pn_open_rule(PN_PIPE_R, 1, 0, 0, 0), PN_NOT_FOUND); /* a terminal is no pipe */
    CHECK_INT(pn_open_rule(PN_PIPE_R, 0, 0, 0, 0), PN_NOT_FOUND);
}

/* a pipe is no terminal: no slave opens on its id, and a master id is taken */
static void terminal_opens_on_a_pipe_id_are_refused(void)
{
    CHECK_INT(pn_open_rule(PN_SLAVE, 1, 1, 0, 0), PN_NOT_FOUND);
    CHECK_INT(pn_open_rule(PN_MASTER, 1, 1, 0, 0), PN_IN_USE);
    CHECK_INT(pn_open_rule(PN_SLAVE, 1, 0, 0, 0), PN_OPEN);
    CHECK_INT(pn_open_rule(PN_SLAVE, 1, 0, 1, 0), PN_NOT_FOUND); /* hung up */
    CHECK_INT(pn_open_rule(PN_MASTER, 0, 0, 0, 0), PN_NEW);
}

void suite_pty_name(void)
{
    pipe_ends_parse_as_a_pipe();
    terminal_names_and_bad_names_are_unchanged();
    pipe_open_rules();
    terminal_opens_on_a_pipe_id_are_refused();
}
