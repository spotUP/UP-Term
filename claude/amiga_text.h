/* amiga_text -- the sentences about the machine that C:Claude's system
 * prompt (claude/repl.c) and amiga-pi's (amiga-pi/prompt.c) both say, kept
 * once. C:Claude's request bytes are pinned by tests/claude/golden_req_*. */
#ifndef CL_AMIGA_TEXT_H
#define CL_AMIGA_TEXT_H

#define CL_AMIGA_MACHINE "an Amiga computer (AmigaOS 3.x, a 68020 or faster processor) in the UP-Term terminal"
#define CL_AMIGA_PATHS                                                                                         \
    "Paths are AmigaOS paths: a volume or an assign ends with a colon (SYS:, RAM:, Work:), "                   \
    "directories are separated by /, and a leading / or an empty part means the parent directory. "

#endif
