/* handler/closeeof.h: the end a read sees after the close gadget. */
#include "harness.h"
#include "../handler/closeeof.h"

void suite_closeeof(void);
void suite_closeeof(void)
{
    int eof;

    /* the close gadget on a line-mode window: the first read sees the end,
     * and so does the next one (vsh read again after mg; the second read
     * waited forever and the window stayed until a second click) */
    eof = 1;
    CHECK_INT(closeeof_read(&eof, 1, 0, 0), 1);
    CHECK_INT(eof, 0);
    CHECK_INT(closeeof_read(&eof, 1, 0, 0), 1);
    CHECK_INT(closeeof_read(&eof, 1, 0, 0), 1);
    CHECK_INT(closeeof_sticky(1, 0, 0), 1);

    /* a one-shot end without a close (Ctrl-\ on a line): one read only */
    eof = 1;
    CHECK_INT(closeeof_read(&eof, 0, 0, 0), 1);
    CHECK_INT(closeeof_read(&eof, 0, 0, 0), 0);
    CHECK_INT(closeeof_sticky(0, 0, 0), 0);

    /* raw window: the close is a break or IECLASS_CLOSEWINDOW, reads wait */
    eof = 0;
    CHECK_INT(closeeof_read(&eof, 1, 1, 0), 0);
    CHECK_INT(closeeof_sticky(1, 1, 0), 0);

    /* AUTO window: the next read opens it again, the end is not kept */
    eof = 1;
    CHECK_INT(closeeof_read(&eof, 1, 0, 1), 1);
    CHECK_INT(closeeof_read(&eof, 1, 0, 1), 0);
}
