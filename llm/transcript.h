/* transcript -- pi-ai's utils/transcript.js and utils/text.js for the backends that take the transcript's system
 * messages in place (anthropic-messages and openai-responses with supportsMidConvoSystemMessages): the text of a
 * system message, the update it renders, and the tool state the later system messages leave (getCurrentTools,
 * resolveTranscriptTools). One implementation; llm/be_anthropic_pi.c and llm/be_openai_responses.c call it.
 * Portable C89. */
#ifndef LLM_TRANSCRIPT_H
#define LLM_TRANSCRIPT_H

#include "backend.h"

/* the inner text of a JSON string value (between the quotes), appended raw */
void llm_tx_inner(jw *w, jv s);
/* pi's contentText: a string, or the text blocks joined by "\n" (inner form, no quotes); the number of parts */
int llm_tx_content_text(jw *w, jv c);
/* pi's getSystemMessageText of a system message: the content, then the sections' texts, the non-empty ones joined by
 * a blank line; the quoted JSON string appended */
void llm_tx_sys_text(jw *w, jv m);
/* pi's renderSystemMessageUpdate as a quoted string; 0 (nothing appended) when it has nothing to say */
long llm_tx_sys_update(jw *w, jv m);

/* the transcript of a request: the initial system message and tools, and the tools after every later system message.
 * The initial system message is msgs[0] when it is one and the model takes system messages in the conversation
 * (mid_sys); else c->system and c->tools stand for it (pi's normalizeContext, or collapseSystemMessages) */
typedef struct llm_tx {
    int has_init;               /* msgs[0] is the initial system message */
    jv init;
    jv *itl;                    /* the initial tools */
    int nitl;
    jv *tl;                     /* the tools after every later system message (getCurrentTools) */
    int ntl;
    int additive;               /* no tool was removed or declared twice (pi: not hasNonAdditiveToolChanges) */
} llm_tx;

/* 0; -1 out of memory or a tool without a name; llm_tx_free either way */
int llm_tx_plan(const llm_ctx *c, int mid_sys, llm_tx *t);
void llm_tx_free(llm_tx *t);

/* a JSON value written compact (JSON.stringify of the parsed value); drop_empty_keys: members with the key "" left out */
void llm_json_compact(jw *w, jv v, int drop_empty_keys);

/* a JSON string value that is blank (empty after JS String.prototype.trim()) */
int llm_tx_blank(jv s);

#endif
