/* The broken-pipe rule of vsh's pipes (shell/sh_pipe.h): the writer is
 * broken when it writes to the closed pipe, never for finishing normally.
 * Deterministic: the order of reader close, writer data and writer end is
 * set by the test, not by timing. */
#include "harness.h"
#include "../shell/sh_pipe.h"

static int rd_tag, wr_tag, job_tag;

static void fresh(sh_pipe_rec *p)
{
    sp_init(p, &rd_tag, &wr_tag);
    p->writer = &job_tag;
}

/* `(exit 2) | true`: the reader closes first, the writer never writes and
 * ends: the pipefail status 3 must not become 130 */
static void writer_that_writes_nothing_is_not_broken_when_the_reader_closes(void)
{
    sh_pipe_rec p;
    fresh(&p);
    sp_reader_closed(&p);
    CHECK_INT(sp_drained(&p, 0), 0); /* the drain only saw end of file */
    sp_writer_gone(&p);
    CHECK_INT(p.broke, 0);
}

/* `yes | true`: the writer writes after the reader closed */
static void writer_that_writes_to_the_closed_pipe_is_broken_once(void)
{
    sh_pipe_rec p;
    fresh(&p);
    sp_reader_closed(&p);
    CHECK_INT(sp_drained(&p, 512), 1);
    CHECK_INT(sp_drained(&p, 512), 0); /* one break per pipe */
}

/* data seen while the reader is still open is not a broken pipe, and a
 * writer that has ended (its end closed) has nobody to break */
static void no_break_without_closed_reader_or_running_writer(void)
{
    sh_pipe_rec p;
    fresh(&p);
    CHECK_INT(sp_drained(&p, 100), 0);
    fresh(&p);
    sp_reader_closed(&p);
    sp_writer_gone(&p);
    CHECK_INT(sp_drained(&p, 100), 0);
    CHECK_INT(p.broke, 0);
}

/* coproc { read x; }: the coproc ends without reading all of its input; the shell
 * writes that pipe itself (no writer job) and has not closed NAME[1]: closing the read
 * end must not drain it (the drain waited for an end of file that never came) */
static void reader_closing_a_pipe_the_shell_writes_does_not_drain(void)
{
    sh_pipe_rec p;
    sp_init(&p, &rd_tag, &wr_tag);
    CHECK_INT(sp_reader_closed(&p), 0);
    fresh(&p);
    CHECK_INT(sp_reader_closed(&p), 1);
}

void suite_sh_pipe(void)
{
    reader_closing_a_pipe_the_shell_writes_does_not_drain();
    writer_that_writes_nothing_is_not_broken_when_the_reader_closes();
    writer_that_writes_to_the_closed_pipe_is_broken_once();
    no_break_without_closed_reader_or_running_writer();
}
