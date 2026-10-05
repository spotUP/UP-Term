/* uptelnet's protocol (net/tn.c; ledger A1.1): option negotiation, NAWS,
 * TERMINAL-TYPE, IAC escaping and the NVT CR rules. */
#include "harness.h"
#include "../net/tn.h"
#include "../engine/vtengine.h"
#include <string.h>

static unsigned char sent[1024], shown[1024];
static int nsent, nshown;

static void cap_out(void *u, const unsigned char *b, int n)
{
    (void)u;
    if (nsent + n <= (int)sizeof(sent)) {
        memcpy(sent + nsent, b, (size_t)n);
        nsent += n;
    }
}

static void cap_show(void *u, const unsigned char *b, int n)
{
    (void)u;
    if (nshown + n <= (int)sizeof(shown)) {
        memcpy(shown + nshown, b, (size_t)n);
        nshown += n;
    }
}

static void clear(void)
{
    nsent = nshown = 0;
}

static void fresh(tn *t, int cols, int rows)
{
    tn_init(t, "xterm-256color", cols, rows, cap_out, cap_show, 0);
    clear();
}

/* feed one byte at a time: the parser's state must survive any split */
static void recv_bytewise(tn *t, const unsigned char *b, int n)
{
    int i;
    for (i = 0; i < n; i++)
        tn_recv(t, b + i, 1);
}

#define SENT_IS(want) do { static const unsigned char w_[] = want; h_checks++; \
    if (nsent != (int)sizeof(w_) - 1 || memcmp(sent, w_, sizeof(w_) - 1)) { h_failures++; \
    printf("  FAIL %s:%d: sent %d bytes, want %d\n", __FILE__, __LINE__, nsent, (int)sizeof(w_) - 1); } } while (0)
#define SHOWN_IS(want) do { static const unsigned char w_[] = want; h_checks++; \
    if (nshown != (int)sizeof(w_) - 1 || memcmp(shown, w_, sizeof(w_) - 1)) { h_failures++; \
    printf("  FAIL %s:%d: shown %d bytes, want %d\n", __FILE__, __LINE__, nshown, (int)sizeof(w_) - 1); } } while (0)

static void opening_offers_naws_ttype_binary_and_asks_sga(void)
{
    tn t;
    fresh(&t, 80, 24);
    tn_start(&t);
    SENT_IS("\377\373\037\377\373\030\377\373\000\377\375\000\377\375\003");
}

/* The server agrees to what we offered: no answer to an answer (no loop),
 * and NAWS sends the size the moment it is on. */
static void agreement_to_our_offer_is_not_answered_again(void)
{
    tn t;
    static const unsigned char yes[] = { 255, 253, 31, 255, 253, 24, 255, 253, 0, 255, 251, 0, 255, 251, 3 };
    fresh(&t, 80, 24);
    tn_start(&t);
    clear();
    tn_recv(&t, yes, (int)sizeof(yes));
    SENT_IS("\377\372\037\000\120\000\030\377\360"); /* only NAWS 80 x 24 */
    CHECK(tn_us(&t, TN_NAWS) && tn_us(&t, TN_TTYPE) && tn_us(&t, TN_BINARY));
    CHECK(tn_him(&t, TN_BINARY) && tn_him(&t, TN_SGA));
    clear();
    tn_recv(&t, yes, (int)sizeof(yes)); /* said again: still nothing */
    CHECK_INT(nsent, 0);
}

static void server_requests_are_answered_once(void)
{
    tn t;
    static const unsigned char will_echo[] = { 255, 251, 1 };
    static const unsigned char do_naws[] = { 255, 253, 31 };
    fresh(&t, 132, 50);
    recv_bytewise(&t, will_echo, 3);
    SENT_IS("\377\375\001"); /* DO ECHO */
    CHECK(tn_him(&t, TN_ECHO));
    clear();
    recv_bytewise(&t, will_echo, 3);
    CHECK_INT(nsent, 0);
    recv_bytewise(&t, do_naws, 3);
    SENT_IS("\377\373\037\377\372\037\000\204\000\062\377\360"); /* WILL NAWS, then 132 x 50 */
}

static void unknown_options_are_refused(void)
{
    tn t;
    static const unsigned char req[] = { 255, 253, 39, 255, 251, 34, 255, 253, 200, 255, 252, 5 };
    fresh(&t, 80, 24);
    tn_recv(&t, req, (int)sizeof(req));
    /* DO NEW-ENVIRON -> WONT, WILL LINEMODE -> DONT, DO 200 -> WONT, WONT 5 ignored */
    SENT_IS("\377\374\047\377\376\042\377\374\310");
}

static void refusal_turns_an_option_off_with_one_reply(void)
{
    tn t;
    static const unsigned char will_echo[] = { 255, 251, 1 };
    static const unsigned char wont_echo[] = { 255, 252, 1 };
    static const unsigned char dont_sga_unasked[] = { 255, 254, 3 };
    fresh(&t, 80, 24);
    tn_recv(&t, will_echo, 3);
    clear();
    tn_recv(&t, wont_echo, 3);
    SENT_IS("\377\376\001"); /* DONT ECHO, acknowledging */
    CHECK(!tn_him(&t, TN_ECHO));
    clear();
    tn_recv(&t, wont_echo, 3);
    CHECK_INT(nsent, 0);
    tn_recv(&t, dont_sga_unasked, 3); /* already off: no reply */
    CHECK_INT(nsent, 0);
}

static void terminal_type_is_answered_when_asked(void)
{
    tn t;
    static const unsigned char do_tt[] = { 255, 253, 24 };
    static const unsigned char send[] = { 255, 250, 24, 1, 255, 240 };
    fresh(&t, 80, 24);
    tn_recv(&t, send, 6); /* not agreed yet: no answer */
    CHECK_INT(nsent, 0);
    tn_recv(&t, do_tt, 3);
    clear();
    recv_bytewise(&t, send, 6);
    SENT_IS("\377\372\030\000xterm-256color\377\360");
}

/* A size whose bytes include 255 doubles them (RFC 1073), and a resize is
 * sent only when NAWS is on and the size changed. */
static void naws_doubles_255_and_follows_resizes(void)
{
    tn t;
    static const unsigned char do_naws[] = { 255, 253, 31 };
    fresh(&t, 80, 24);
    tn_size(&t, 100, 30); /* NAWS off: nothing */
    CHECK_INT(nsent, 0);
    tn_recv(&t, do_naws, 3);
    clear();
    tn_size(&t, 255, 511);
    SENT_IS("\377\372\037\000\377\377\001\377\377\377\360");
    clear();
    tn_size(&t, 255, 511);
    CHECK_INT(nsent, 0);
}

static void data_iac_iac_is_one_255_even_split(void)
{
    tn t;
    static const unsigned char in[] = { 'a', 255, 255, 'b', 255, 241, 'c' }; /* IAC NOP dropped */
    fresh(&t, 80, 24);
    recv_bytewise(&t, in, (int)sizeof(in));
    SHOWN_IS("a\377bc");
}

/* UTF-8 passes byte for byte (the server's xterm-256color screen). */
static void utf8_passes_untouched(void)
{
    tn t;
    static const unsigned char in[] = "\342\217\272 hello \342\234\273";
    fresh(&t, 80, 24);
    tn_recv(&t, in, (int)sizeof(in) - 1);
    SHOWN_IS("\342\217\272 hello \342\234\273");
}

static void subnegotiation_payload_never_shows(void)
{
    tn t;
    static const unsigned char in[] = { 'x', 255, 250, 39, 1, 'A', 255, 255, 'B', 255, 240, 'y' };
    fresh(&t, 80, 24);
    recv_bytewise(&t, in, (int)sizeof(in));
    SHOWN_IS("xy");
}

static void nvt_cr_nul_shows_as_cr_binary_keeps_nul(void)
{
    tn t;
    static const unsigned char in[] = { 'a', '\r', 0, 'b', '\r', '\n' };
    static const unsigned char will_bin[] = { 255, 251, 0 };
    fresh(&t, 80, 24);
    tn_recv(&t, in, (int)sizeof(in));
    SHOWN_IS("a\rb\r\n");
    tn_recv(&t, will_bin, 3);
    nshown = 0;
    tn_recv(&t, in, (int)sizeof(in));
    CHECK_INT(nshown, 6);
}

static void typed_iac_doubles_and_cr_follows_binary(void)
{
    tn t;
    static const unsigned char typed[] = { 'h', 255, '\r' };
    static const unsigned char do_bin_will_echo[] = { 255, 253, 0, 255, 251, 1 };
    fresh(&t, 80, 24);
    tn_send(&t, typed, 3);
    SENT_IS("h\377\377\r\000"); /* NVT: CR NUL */
    SHOWN_IS("h\377\r\n");       /* no server echo yet: echoed here */
    tn_recv(&t, do_bin_will_echo, 6);
    clear();
    tn_send(&t, typed, 3);
    SENT_IS("h\377\377\r");      /* BINARY: CR alone; the server echoes */
    CHECK_INT(nshown, 0);
}

/* the owner's screen 2026-10-05: tmux's alternate screen, scroll region and
 * mouse mode outlived a closed connection; tn_goodbye undoes them, and the
 * cursor stays put (the first try homed it through DECSTBM) */
static void session_end_gives_the_shell_its_screen_back(void)
{
    static const unsigned char tmux[] = "\033[?1049h\033[3;10r\033[?25l\033[?1000h\033[?1006h"
                                        "\033[?1004h\033[?2004h\033[?1h\033=\033[?2026h\033[1;7m";
    static const char before[] = "line one\r\nline two\r\nPassword: ";
    vt_term *v = vt_new(80, 24, 0, 0, 0);
    const char *bye;
    tn t;
    vt_u32 m;
    int x0, y0, x, y;
    CHECK(v != 0);
    if (!v)
        return;
    vt_write(v, (const vt_u8 *)before, (long)strlen(before));
    vt_cursor(v, &x0, &y0);
    /* the far side's screen, through tn so it is tracked */
    fresh(&t, 80, 24);
    recv_bytewise(&t, tmux, (int)sizeof(tmux) - 1);
    vt_write(v, tmux, (long)sizeof(tmux) - 1);
    m = vt_modes(v);
    CHECK((m & VT_MODE_ALT_SCREEN) && (m & VT_MODE_MOUSE_NORMAL) && !(m & VT_MODE_CURSOR_VISIBLE));
    bye = tn_goodbye(&t);
    vt_write(v, (const vt_u8 *)bye, (long)strlen(bye));
    m = vt_modes(v);
    CHECK(!(m & VT_MODE_ALT_SCREEN));
    CHECK(m & VT_MODE_CURSOR_VISIBLE);
    CHECK(!(m & (VT_MODE_MOUSE_X10 | VT_MODE_MOUSE_NORMAL | VT_MODE_MOUSE_BUTTON | VT_MODE_MOUSE_ANY |
                 VT_MODE_MOUSE_SGR | VT_MODE_MOUSE_UTF8 | VT_MODE_MOUSE_URXVT)));
    CHECK(!(m & (VT_MODE_FOCUS | VT_MODE_BRACKET_PASTE | VT_MODE_APP_CURSOR | VT_MODE_APP_KEYPAD |
                 VT_MODE_SYNC)));
    vt_cursor(v, &x, &y);
    CHECK_INT(y, y0);   /* back where the main screen was left */
    CHECK_INT(x, x0);
    vt_free(v);
}

/* a session that never switched screens: the cursor stays where its
 * output ended, the main screen is not "restored" */
static void session_end_on_the_main_screen_keeps_the_cursor(void)
{
    static const unsigned char out[] = "\033[5;20r\033[20;1Hhello\r\nworld";
    vt_term *v = vt_new(80, 24, 0, 0, 0);
    const char *bye;
    tn t;
    int x0, y0, x, y;
    CHECK(v != 0);
    if (!v)
        return;
    fresh(&t, 80, 24);
    recv_bytewise(&t, out, (int)sizeof(out) - 1);
    vt_write(v, out, (long)sizeof(out) - 1);
    vt_cursor(v, &x0, &y0);
    bye = tn_goodbye(&t);
    CHECK(strstr(bye, "1049") == 0);
    vt_write(v, (const vt_u8 *)bye, (long)strlen(bye));
    vt_cursor(v, &x, &y);
    CHECK_INT(y, y0);
    CHECK_INT(x, x0);
    vt_free(v);
}

void suite_telnet(void)
{
    opening_offers_naws_ttype_binary_and_asks_sga();
    agreement_to_our_offer_is_not_answered_again();
    server_requests_are_answered_once();
    unknown_options_are_refused();
    refusal_turns_an_option_off_with_one_reply();
    terminal_type_is_answered_when_asked();
    naws_doubles_255_and_follows_resizes();
    data_iac_iac_is_one_255_even_split();
    utf8_passes_untouched();
    subnegotiation_payload_never_shows();
    nvt_cr_nul_shows_as_cr_binary_keeps_nul();
    typed_iac_doubles_and_cr_follows_binary();
    session_end_gives_the_shell_its_screen_back();
    session_end_on_the_main_screen_keeps_the_cursor();
}
