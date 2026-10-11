/* jumpscroll.h -- jump scroll's bookkeeping (S1, creep's CCON 1.2.8), pure,
 * no OS calls, so the host suite can pin it (tests/test_pace.c).
 *
 * While whole-screen scrolls keep coming frame after frame -- a flood of
 * output, or a program that waits for each line to be drawn (conbench
 * sync-line) -- the screen moves further than the grid did and keeps the
 * difference as `jump`: screen row s shows grid row s + jump, the rows below
 * the text are blank, and the next scrolls move no pixels until the text
 * reaches the bottom again. Each render pass that scrolls right after one
 * that scrolled doubles the step, up to a screenful less one row.
 *
 * A key from the user ends the run (vt_jump_key): what comes next answers
 * that key, and the screen follows it line by line. Without that, Return
 * held at a shell prompt -- a line every 50 ms of key repeat, each one a
 * render pass that scrolled -- counted as a flood: the step grew to a
 * screenful, the line being typed was drawn on the window's top row with
 * the rows under it blank, and every repeat carried on from there down to
 * the bottom, then up to the top again (owner, FS-UAE rig, 2026-10-12: "it
 * starts from the top when it reaches the bottom"; rig trace: jump 10 of 11
 * rows at the Return key, every value 10..0 in turn). A program that waits
 * for each line to be drawn (sync-line) types no keys and still jumps. */
#ifndef VT_JUMPSCROLL_H
#define VT_JUMPSCROLL_H

typedef struct vt_jump {
    short jump;              /* screen row s shows grid row s + jump */
    short step;              /* the spare rows the next jump leaves */
    unsigned char scrolled;  /* this pass scrolled the whole screen */
} vt_jump;

/* A render pass begins on a screen of `rows` text rows: a pass right after
 * one that scrolled moves twice as far, any other resets the step. */
static void vt_jump_pass(vt_jump *j, int rows)
{
    int h = rows - 1;
    if (j->scrolled) {
        j->step = (short)(j->step ? j->step * 2 : 2);
        if (j->step > h)
            j->step = (short)(h > 0 ? h : 0);
    } else {
        j->step = 0;
    }
    j->scrolled = 0;
}

/* A key from the user: the next scroll does not continue a flood. A jump
 * already made stays (the text moves into its blank rows, vr_settle ends it). */
static void vt_jump_key(vt_jump *j)
{
    j->scrolled = 0;
    j->step = 0;
}

/* The whole screen of `rows` rows up by n inside a pass: the pixel rows to
 * move it up by (0: none, the text moves into the blank rows below it). */
static int vt_jump_scroll(vt_jump *j, int rows, int n)
{
    int b;
    j->scrolled = 1;
    if (j->jump >= n) {
        j->jump = (short)(j->jump - n);
        return 0;
    }
    b = n - j->jump + j->step; /* move that far, keep step rows to spare */
    if (b > rows)
        b = rows;
    j->jump = (short)(j->jump + b - n);
    if (j->jump < 0)
        j->jump = 0;
    return b;
}

#endif
