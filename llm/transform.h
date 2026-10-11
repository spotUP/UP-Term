/* transform -- the messages one request carries (amiga-pi plan C11), after
 * pi-ai's transform-messages.ts second pass (pi f10993bc :158-240): an
 * assistant message that stopped with error or aborted is left out (it may
 * hold half a tool call; the model goes on from the last good state), and a
 * tool call no toolResult answers before the next assistant or user message
 * (or the end) gets one: isError, text "No result provided". A system
 * message between a call and its results waits until after them.
 * Portable C89, host-tested (amiga-pi tests/test_pi_agent.c). */
#ifndef LLM_TRANSFORM_H
#define LLM_TRANSFORM_H

#include "llm.h"

typedef struct llm_view {
    const char **m;             /* the messages to send: the caller's or synthesized ones */
    long *n;
    int count, cap;
    char **own;                 /* the synthesized ones (freed with the view) */
    int nown, owncap;
    int synth;                  /* results synthesized */
    int dropped;                /* assistant messages left out */
} llm_view;

/* 0, -1 out of memory or a message that is not JSON; ts stamps the synthesized results */
int llm_transform(const char *const *msgs, const long *n, int count, llm_ts ts, llm_view *v);
void llm_view_free(llm_view *v);

#endif
