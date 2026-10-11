/* pi-llmprobe -- amiga-pi's LLM layer on the Amiga (plan B12): one prompt
 * through llm_request (llm/retry.c) over bsdsocket.library, the answer's
 * text printed as it streams, then the stop reason.
 *
 *   pi-llmprobe BASEURL MODEL [KEY] [PROMPT]
 *   pi-llmprobe http://127.0.0.1:8092/v1 qwen2.5-coder:7b sk-not-sent hello
 *
 * API openai-completions, provider "fixture". A KEY given with a plain
 * http URL must not be sent (the notice says so); tools/rig/amigapi_rig.py
 * checks that against tools/claude_fixture.py's log. Exit 0 when the
 * answer completed, 10 when it failed. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../../claude/net_amiga.h"
#include "../../llm/request.h"
#include "../../llm/accum.h"

static void out(const char *s, long n)
{
    fwrite(s, 1, (size_t)n, stdout);
    fflush(stdout);
}

/* the answer's text to the screen as it comes, everything to the accumulator */
static llm_acc acc;
static long text_index = -1;

static void on_ev(void *u, const llm_event *e)
{
    (void)u;
    if (e->type == LE_BLOCK_START && e->kind == LB_TEXT)
        text_index = e->index;
    if (e->type == LE_DELTA && e->index == text_index)
        out(e->p, e->n);
    llm_acc_event(&acc, e);
}

static int h_poll(void *u)
{
    (void)u;
    return (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0;
}

static int h_sleep(void *u, long ms)
{
    (void)u;
    Delay(ms / 20 > 0 ? ms / 20 : 1);
    return h_poll(u);
}

static void h_notice(void *u, const char *t)
{
    (void)u;
    printf("[INFO] %s\n", t);
}

static int h_resolve(void *u, const char *host, unsigned char ip[4])
{
    (void)u;
    return net_amiga_resolve(host, ip);
}

int main(int argc, char **argv)
{
    static net_amiga na;
    static cl_net net;
    static llm_pool pool;
    cl_net *nets[1];
    llm_model m;
    llm_ctx c;
    llm_req q;
    llm_sink k;
    jw msg, content;
    const char *msgs[1];
    long msg_n[1];
    llm_ts ts;
    int rc;
    if (argc < 3) {
        printf("usage: pi-llmprobe BASEURL MODEL [KEY] [PROMPT]\n");
        return 20;
    }
    net_amiga_init(&na, &net);
    nets[0] = &net;
    llm_pool_init(&pool, nets, 1);
    llm_model_init(&m);
    m.id = argv[2];
    m.api = "openai-completions";
    m.provider = "fixture";
    m.base_url = argv[1];
    ts.sec = 0;
    ts.ms = 0;
    jw_init(&content);
    jw_strz(&content, argc > 4 ? argv[4] : "hello");
    jw_init(&msg);
    llm_m_user(&msg, content.p, content.n, ts);
    msgs[0] = msg.p;
    msg_n[0] = msg.n;
    memset(&c, 0, sizeof(c));
    c.model = &m;
    c.system = "You are amiga-pi's probe.";
    c.msgs = msgs;
    c.msg_n = msg_n;
    c.nmsg = 1;
    llm_acc_init(&acc, m.api, m.provider);
    k.u = 0;
    k.ev = on_ev;
    memset(&q, 0, sizeof(q));
    q.pool = &pool;
    q.model = &m;
    q.be = &llm_be_openai_chat;
    q.ctx = &c;
    q.key = argc > 3 ? argv[3] : 0;
    q.user_agent = "UP-Term-amiga-pi/0.1 (AmigaOS)";
    q.h.poll = h_poll;
    q.h.sleep = h_sleep;
    q.h.notice = h_notice;
    q.h.resolve = h_resolve;
    q.sink = &k;
    rc = llm_request(&q);
    {
        jw a;
        llm_info inf;
        jw_init(&a);
        llm_acc_message(&acc, ts, &a);
        llm_msg_info(a.p, a.n, &inf);
        printf("\n[%s] stop=%s attempts=%d key_sent=%d\n", rc ? "ERROR" : "DONE", llm_stop_name(inf.stop),
               q.attempts, q.key_sent);
        if (rc && acc.err[0])
            printf("[ERROR] %s\n", acc.err);
        jw_free(&a);
    }
    llm_acc_free(&acc);
    llm_pool_close(&pool);
    net_amiga_exit(&na);
    jw_free(&msg);
    jw_free(&content);
    return rc ? 10 : 0;
}
