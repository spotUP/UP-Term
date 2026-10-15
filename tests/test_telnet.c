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

/* ---- the key login (NL7; the protocol is in net/tn.h) ---- */

static const unsigned char K_[32] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                      24, 25, 26, 27, 28, 29, 30, 31 };
static const unsigned char CN_[16] = { 0x20, 0xff, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
                                       0x2d, 0x2e, 0x2f };
/* the server's nonce has a 255 first and a 254 last: the 255 is doubled on the wire */
#define SEND_MSG "\377\372\045\001\165\377\377\001\002\003\004\005\006\007\010\011\012\013\014\015\016\376\377\360"
/* the client's answer, made with HMAC-SHA256 outside this code (Python's hmac): ID "amiga-1", cn above */
#define IS_MSG "\377\372\045\000\165\141\155\151\147\141\055\061\000\040\377\377\042\043\044\045\046\047\050\051" \
    "\052\053\054\055\056\057\175\327\001\062\243\342\114\042\131\012\272\176\344\012\311\357\256\071\355\265" \
    "\020\015\163\253\252\363\175\130\225\322\140\322\377\360"
/* the server's proof for the same exchange */
#define ACCEPT_MSG "\377\372\045\002\165\001\256\312\307\236\150\145\166\342\302\124\261\217\260\321\261\206\133\211" \
    "\357\071\130\162\345\341\151\274\047\213\365\051\333\330\377\360"

static int ev_state, ev_n;
static char ev_id[20];
static unsigned char ev_key[32];

static void on_auth(void *u, int state, const char *id, const unsigned char *key)
{
    (void)u;
    ev_state = state;
    ev_n++;
    if (id) {
        memcpy(ev_id, id, strlen(id) + 1);
        memcpy(ev_key, key, 32);
    }
}

static void keyed(tn *t)
{
    fresh(t, 80, 24);
    tn_set_key(t, "amiga-1", K_, CN_, on_auth);
    ev_state = ev_n = 0;
}

static void key_login_answers_the_challenge_with_the_exact_bytes(void)
{
    tn t;
    keyed(&t);
    tn_recv(&t, (const unsigned char *)"\377\375\045", 3);
    SENT_IS("\377\373\045");                       /* DO AUTH answered with WILL AUTH */
    CHECK_INT(tn_auth(&t), TN_AUTH_ASKED);
    clear();
    tn_recv(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);
    SENT_IS(IS_MSG);                               /* the nonces' 255 doubled, mac from the fixed vector */
    CHECK_INT(tn_auth(&t), TN_AUTH_SENT);
    clear();
    tn_recv(&t, (const unsigned char *)ACCEPT_MSG, (int)sizeof(ACCEPT_MSG) - 1);
    CHECK_INT(tn_auth(&t), TN_AUTH_OK);
    CHECK_INT(ev_state, TN_AUTH_OK);
    CHECK_INT(nsent, 0);
    tn_recv(&t, (const unsigned char *)"hello", 5);
    SHOWN_IS("hello");                             /* the session goes on */
}

static void the_session_daemon_port_is_announced_only_after_the_proof(void)
{
    tn t;
    keyed(&t);
    tn_announce_agent(&t, 7847);
    CHECK_INT(nsent, 0);                           /* not logged in by key: nothing goes out */
    tn_recv(&t, (const unsigned char *)"\377\375\045", 3);
    tn_recv(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);
    tn_recv(&t, (const unsigned char *)ACCEPT_MSG, (int)sizeof(ACCEPT_MSG) - 1);
    clear();
    tn_announce_agent(&t, 7847);                   /* 0x1ea7 */
    SENT_IS("\377\372\045\000\165\001\036\247\377\360");
    clear();
    tn_announce_agent(&t, 255);                    /* the 255 in the port is doubled */
    SENT_IS("\377\372\045\000\165\001\000\377\377\377\360");
}

static void key_login_split_at_every_byte_is_the_same(void)
{
    tn t;
    keyed(&t);
    recv_bytewise(&t, (const unsigned char *)"\377\375\045", 3);
    clear();
    recv_bytewise(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);
    SENT_IS(IS_MSG);
    recv_bytewise(&t, (const unsigned char *)ACCEPT_MSG, (int)sizeof(ACCEPT_MSG) - 1);
    CHECK_INT(tn_auth(&t), TN_AUTH_OK);
}

static void a_bad_server_proof_closes_the_line(void)
{
    tn t;
    static unsigned char bad[sizeof(ACCEPT_MSG)];
    keyed(&t);
    tn_recv(&t, (const unsigned char *)"\377\375\045", 3);
    tn_recv(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);
    memcpy(bad, ACCEPT_MSG, sizeof(ACCEPT_MSG));
    bad[20] ^= 1;                                  /* one bit of smac */
    clear();
    tn_recv(&t, bad, (int)sizeof(ACCEPT_MSG) - 1);
    CHECK_INT(tn_auth(&t), TN_AUTH_BAD_SERVER);
    CHECK_INT(ev_state, TN_AUTH_BAD_SERVER);
    tn_recv(&t, (const unsigned char *)"Password: ", 10);
    CHECK_INT(nshown, 0);                          /* nothing more is shown or answered */
    tn_recv(&t, (const unsigned char *)"\377\375\003", 3);
    CHECK_INT(nsent, 0);
    /* a server that ACCEPTs before any challenge, or with a short proof, fails too */
    keyed(&t);
    tn_recv(&t, (const unsigned char *)"\377\375\045", 3);
    tn_recv(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);
    tn_recv(&t, (const unsigned char *)"\377\372\045\002\165\001\001\002\377\360", 10);
    CHECK_INT(tn_auth(&t), TN_AUTH_BAD_SERVER);
}

static void a_rejected_key_falls_back_to_the_password(void)
{
    tn t;
    keyed(&t);
    tn_recv(&t, (const unsigned char *)"\377\375\045", 3);
    tn_recv(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);
    tn_recv(&t, (const unsigned char *)"\377\372\045\002\165\000\377\360Password: ", 18);
    CHECK_INT(tn_auth(&t), TN_AUTH_REJECTED);
    CHECK_INT(ev_state, TN_AUTH_REJECTED);
    SHOWN_IS("Password: ");
}

static void pairing_stores_the_key_the_server_hands_over(void)
{
    tn t;
    static const unsigned char pair[] = "\377\372\045\002\165\002" "amiga-9\0"
        "\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217"
        "\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\377\377" "\377\360";
    unsigned char want[32];
    int i;
    fresh(&t, 80, 24);
    tn_set_key(&t, 0, 0, CN_, on_auth);            /* no key file yet: ask to be paired */
    ev_state = ev_n = 0;
    tn_recv(&t, (const unsigned char *)"\377\375\045", 3);
    clear();
    tn_recv(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);
    /* ID empty, no mac: IS 0x75 0x00 cn[16] */
    SENT_IS("\377\372\045\000\165\000\040\377\377\042\043\044\045\046\047\050\051\052\053\054\055\056\057\377\360");
    clear();
    tn_recv(&t, (const unsigned char *)"Password: ", 10);
    SHOWN_IS("Password: ");
    for (i = 0; i < 31; i++)
        want[i] = (unsigned char)(0x80 + i);
    want[31] = 0xff;
    recv_bytewise(&t, pair, (int)sizeof(pair) - 1);
    CHECK_INT(tn_auth(&t), TN_AUTH_PAIRED);
    CHECK_INT(ev_n, 1);
    CHECK_STR(ev_id, "amiga-9");
    CHECK(!memcmp(ev_key, want, 32));              /* the 255 in the key came through undoubled */
    /* a client that has a key never takes a PAIR */
    keyed(&t);
    tn_recv(&t, (const unsigned char *)"\377\375\045", 3);
    tn_recv(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);
    tn_recv(&t, pair, (int)sizeof(pair) - 1);
    CHECK_INT(ev_n, 0);
    CHECK_INT(tn_auth(&t), TN_AUTH_SENT);
}

static void a_client_without_a_key_says_wont_and_nothing_else_changes(void)
{
    tn t;
    fresh(&t, 80, 24);
    tn_recv(&t, (const unsigned char *)"\377\375\045", 3);
    SENT_IS("\377\374\045");                       /* WONT AUTH: today's answer to an option it does not know */
    CHECK_INT(tn_auth(&t), TN_AUTH_OFF);
    clear();
    tn_recv(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);
    CHECK_INT(nsent, 0);                           /* an unasked SEND is ignored */
    /* the opening is byte for byte what it was */
    fresh(&t, 80, 24);
    tn_start(&t);
    SENT_IS("\377\373\037\377\373\030\377\373\000\377\375\000\377\375\003");
    /* a keyed client starts the same way: AUTH is the server's to offer */
    keyed(&t);
    clear();
    tn_start(&t);
    SENT_IS("\377\373\037\377\373\030\377\373\000\377\375\000\377\375\003");
}

static void an_oversize_subnegotiation_is_dropped_not_cut(void)
{
    tn t;
    unsigned char big[200];
    int i;
    keyed(&t);
    tn_recv(&t, (const unsigned char *)"\377\375\045", 3);
    clear();
    big[0] = 255;
    big[1] = 250;
    big[2] = TN_AUTH;
    big[3] = TN_AUTH_SEND;
    big[4] = TN_AUTH_TYPE;
    for (i = 5; i < 198; i++)
        big[i] = (unsigned char)i;
    big[198] = 255;
    big[199] = 240;
    tn_recv(&t, big, 200);
    CHECK_INT(nsent, 0);
    CHECK_INT(tn_auth(&t), TN_AUTH_ASKED);
    tn_recv(&t, (const unsigned char *)SEND_MSG, (int)sizeof(SEND_MSG) - 1);   /* the next one still works */
    CHECK_INT(tn_auth(&t), TN_AUTH_SENT);
}

static void the_key_file_line_round_trips(void)
{
    char id[TN_ID_MAX + 1], line[TN_ID_MAX + 2 * TN_KEY_LEN + 3];
    unsigned char key[32];
    int i;
    CHECK_INT(tn_key_format("amiga-1", K_, line), 7 + 1 + 64 + 1);
    CHECK_STR(line, "amiga-1 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f\n");
    CHECK_INT(tn_key_parse(line, id, key), 1);
    CHECK_STR(id, "amiga-1");
    CHECK(!memcmp(key, K_, 32));
    CHECK_INT(tn_key_parse("amiga-1 000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F", id, key), 1);
    CHECK_INT(tn_key_parse("", id, key), 0);                      /* an empty file asks for a pairing */
    CHECK_INT(tn_key_parse("\n", id, key), 0);
    CHECK_INT(tn_key_parse("amiga-1\n", id, key), 0);
    CHECK_INT(tn_key_parse("amiga-1 0001", id, key), 0);
    CHECK_INT(tn_key_parse("bad/id 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", id, key), 0);
    CHECK_INT(tn_key_parse("amiga-1 00010203040506070809zz0b0c0d0e0f101112131415161718191a1b1c1d1e1f", id, key), 0);
    CHECK_INT(tn_key_parse("a234567890123456789 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", id, key),
              0);                                                 /* an ID of 19 characters */
    for (i = 0; i < 32; i++)
        key[i] = 0;
    CHECK_INT(tn_ct_eq(K_, K_, 32), 1);
    CHECK_INT(tn_ct_eq(K_, key, 32), 0);
}

/* the session daemon's key: the same hex tools/test_uptelnetd.py pins (SESSION_VEC) for K_, the SEND_MSG nonce, CN_ */
static void the_session_key_is_the_vector_the_nas_derives(void)
{
    static const unsigned char sn[16] = { 0xff, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0xfe };
    char hex[65];
    tn_session_key(K_, 32, sn, 16, CN_, 16, hex);
    CHECK(!strcmp(hex, "d83a7b587a7c53f235d33e5804971f7ee49132f59cac9cc9fefcc717528da353"));
    tn_session_key(K_, 32, sn, 16, CN_, 49, hex);
    CHECK_INT(hex[0], 0);
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
    key_login_answers_the_challenge_with_the_exact_bytes();
    the_session_daemon_port_is_announced_only_after_the_proof();
    key_login_split_at_every_byte_is_the_same();
    a_bad_server_proof_closes_the_line();
    a_rejected_key_falls_back_to_the_password();
    pairing_stores_the_key_the_server_hands_over();
    a_client_without_a_key_says_wont_and_nothing_else_changes();
    an_oversize_subnegotiation_is_dropped_not_cut();
    the_key_file_line_round_trips();
    the_session_key_is_the_vector_the_nas_derives();
}
