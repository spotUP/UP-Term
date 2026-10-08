/* The broken-pipe rule of vsh's pipes, apart from AmigaOS so the host tests
 * it (tests/test_sh_pipe.c). AmigaOS has no SIGPIPE: a PIPE: writer whose
 * reader has gone blocks for ever. vsh closes the read end and reads the
 * pipe dry (which lets a blocked Write return); the break (Ctrl-C) goes to
 * the writer only when it writes to the closed pipe, i.e. when the drain
 * gets data while the writer still holds its end. A writer that finishes
 * without writing (`(exit 2) | true`) is never broken: its status stays. */
#ifndef SH_PIPE_H
#define SH_PIPE_H

typedef struct sh_pipe_rec {
    void *rd, *wr;          /* the two stream handles */
    void *writer;           /* the command writing it, if one runs */
    int rd_closed;          /* the reader has gone */
    int broke;              /* the writer got its break */
} sh_pipe_rec;

static void sp_init(sh_pipe_rec *p, void *rd, void *wr)
{
    p->rd = rd;
    p->wr = wr;
    p->writer = 0;
    p->rd_closed = 0;
    p->broke = 0;
}

/* The reader closes. 1: drain the pipe, a command writes it (it may be blocked in a
 * Write the closed reader never takes). 0: no command writes it, the write end is a
 * shell's own (a coproc's input, NAME[1]): a drain would wait for that shell to close
 * it, and a coproc that ended without reading all of it never ended (rig: wait hung). */
static int sp_reader_closed(sh_pipe_rec *p)
{
    p->rd_closed = 1;
    return p->writer != 0;
}

/* The drain read n bytes: 1 when the writer must get its break now (once). */
static int sp_drained(sh_pipe_rec *p, long n)
{
    if (n <= 0 || !p->rd_closed || !p->writer || p->broke)
        return 0;
    p->broke = 1;
    return 1;
}

/* The writer ended or closed its end. */
static void sp_writer_gone(sh_pipe_rec *p)
{
    p->wr = 0;
    p->writer = 0;
}

#endif
