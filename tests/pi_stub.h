/* A stub transport for the LLM layer's host tests (tests/test_pi_llm.c,
 * amiga-pi's tests/test_pi_agent.c): canned HTTP answers from recordings
 * (tests/claude/, tests/pi/ through claude_load), every request's head and
 * body kept, a clock, a break after so many bytes, the llm_hooks it
 * answers (poll, ms, sleep, notice, resolve, log) and models of both APIs.
 * nas.lan resolves to 192.168.1.10, public.example to 8.8.8.8. */
#ifndef PI_STUB_H
#define PI_STUB_H

#include "../llm/request.h"

#define PS_MAX 12

typedef struct pstub {
    char *resp[PS_MAX];
    long rlen[PS_MAX];
    int nresp, next, cur;
    long pos;
    jw req;
    char *head[PS_MAX], *body[PS_MAX];
    int nreq, opens, closes;
    long brk_at;                /* the poll says stop once this many bytes went out */
    int brk;
    int silent;                 /* answer nothing at all (the idle limit) */
    int sleeps;
    long slept;
    int notices;
    char notice[160];
    jw log;
    unsigned long clock;
} pstub;

extern pstub ps;
extern cl_net ps_nets[LLM_POOL];
extern llm_pool ps_pool;

/* everything forgotten (the requests, the answers queued) */
void ps_reset(void);
/* an SSE recording queued as a chunked 200 answer (cut: no last chunk, the connection closes) */
void ps_sse(const char *name, int cut);
/* a whole HTTP answer queued as it is in the file */
void ps_raw(const char *name);
/* ps_pool over n stub transports */
void ps_pool_up(int n);

int ps_poll(void *u);
unsigned long ps_ms(void *u);
int ps_sleep(void *u, long ms);
void ps_notice(void *u, const char *t);
int ps_resolve(void *u, const char *host, unsigned char ip[4]);
void ps_log(void *u, const char *s, long n);

llm_model ps_model_openai(const char *base);   /* openai-completions, provider ollama */
llm_model ps_model_anthropic(void);            /* anthropic-messages */

#endif
