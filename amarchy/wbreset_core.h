/* WBRESET-EXT: Amarchy's windows leave the Workbench screen before any CloseWorkBench and come back after
 * OpenWorkBench (design: thoughts/shared/research/2026-10-31_wbreset-external-use.md). The Amarchy commodity
 * patches intuition CloseWorkBench / OpenWorkBench (wbreset.c) and keeps the public record "Amarchy.WBReset";
 * the bar, amarchy-gui and amarchy-menu are its clients. This file is the pure part: the client table and the
 * phase machine the patch and the clients drive, and the bytes of the gate stub. Host-tested (tests/test_wbreset.c). */
#ifndef AMARCHY_WBRESET_CORE_H
#define AMARCHY_WBRESET_CORE_H

#include <stdint.h>
#include "gthemes_core.h"

#define WBR_SEM_NAME "Amarchy.WBReset"
#define WBR_MAX 8
#define WBR_WAIT_TICKS 150          /* the patch waits at most 3 s for the clients to close their windows */
#define WBR_LVO_CLOSEWB (-78)       /* intuition CloseWorkBench() */
#define WBR_LVO_OPENWB (-210)       /* intuition OpenWorkBench() */

enum { WBR_HIDE = 0, WBR_DISMISS = 1 };                 /* client modes: hide and show again, or close for good */
enum { WBR_OPEN = 0, WBR_CLOSING = 1, WBR_CLOSED = 2 }; /* the record's phase */
enum { WBR_DO_NONE = 0, WBR_DO_HIDE = 1, WBR_DO_SHOW = 2 };

typedef struct wbr_client {
    void *task;                     /* 0 = free slot */
    uint32_t sig;                   /* what the patch Signal()s */
    uint8_t mode;
    uint8_t has_win;                /* 1 while the client holds a window (or a lock) on the Workbench screen */
    uint8_t hidden;                 /* 1 while a HIDE client's windows are closed for a reset */
    uint8_t pad;
} wbr_client;

typedef struct wbr_rec {
    uint16_t phase;
    uint16_t busy;                  /* callers inside the patch (Amarchy's exit waits for 0) */
    uint32_t closes, opens, failed, timeouts;
    int16_t diag_vis, diag_wins;    /* diagnostic: Workbench visitors / windows just before the original CloseWorkBench */
    int32_t diag_ok;                /* diagnostic: what the original CloseWorkBench returned */
    wbr_client c[WBR_MAX];
} wbr_rec;

/* A task and the signal to send it, as listed by the begin / end calls. */
typedef struct wbr_sig {
    void *task;
    uint32_t sig;
} wbr_sig;

void wbr_init(wbr_rec *r);

/* Register or update a client (keyed by task). Returns the slot, or -1 when the table is full or task is 0. */
int wbr_set(wbr_rec *r, void *task, uint32_t sig, int mode, int has_win);
void wbr_drop(wbr_rec *r, void *task);

/* CloseWorkBench, before the original: phase CLOSING; out gets every client with a window except self (the
 * caller's task, which cannot answer while it is inside the patch). Returns the count (at most WBR_MAX). */
int wbr_close_begin(wbr_rec *r, void *self, wbr_sig *out);
/* Clients the patch still waits for: a listed (non-self) client that still has a window. */
int wbr_pending(const wbr_rec *r, void *self);
/* After the original CloseWorkBench: ok -> phase CLOSED, nothing to signal; failed -> phase OPEN and out lists
 * the hidden HIDE clients, which reopen at once. timed_out counts the waits that ran out. Returns the count. */
int wbr_close_end(wbr_rec *r, int ok, int timed_out, wbr_sig *out);
/* After the original OpenWorkBench: ok -> phase OPEN and out lists the hidden HIDE clients. Returns the count. */
int wbr_open_end(wbr_rec *r, int ok, wbr_sig *out);

/* A client woke on its signal: what to do now. DO_HIDE marks it hidden (HIDE mode); the client closes its
 * windows and reports has_win 0 through wbr_set. DO_SHOW clears hidden; the client reopens. */
int wbr_client_step(wbr_rec *r, void *task);

/* The gate stub the JMP operand points at (68000, position independent, big-endian):
 *   0  2F3A 000C   move.l  hook(pc),-(sp)
 *   4  6606        bne.s   go
 *   6  588F        addq.l  #4,sp
 *   8  2F3A 0008   move.l  saved(pc),-(sp)
 *  12  4E75   go:  rts
 *  14  hook  (0 = pass straight to saved)
 *  18  saved
 * Every register and the stack reach hook or saved as the caller left them. */
#define WBR_GATE_SIZE 22
#define WBR_GATE_HOOK 14
#define WBR_GATE_SAVED 18
void wbr_gate_build(unsigned char *out, uint32_t hook, uint32_t saved);


/* TE-7 (plan section 5, decision D1): the theme fields that only a Workbench reset applies. The pure decision: which of them differ
 * between the running model and the one being saved. Screen font, window title font, title bar height, gadget size, gadget padding,
 * gadget order, scroll bar width. Everything else (colours, radius, border width, knob, glyph set, ghost, hover, menu / shell / icon
 * font, pointer) is live and never in this mask. */
enum { WBR_B_SCREEN_FONT = 1, WBR_B_TITLE_FONT = 2, WBR_B_BAR_HEIGHT = 4, WBR_B_GADGET_SIZE = 8, WBR_B_GADGET_PAD = 16,
       WBR_B_GADGET_ORDER = 32, WBR_B_SCROLL_WIDTH = 64 };
unsigned wbr_bound_mask(const gt_model *running, const gt_model *neu);
/* the "Reset Workbench Now / Later" requester rides the DESK CONFIRM verb (desk_confirm_dos.c); a positive int in the verb's number */
#define WBR_ASK_MASK 0x40000000u   /* the DESK CONFIRM mask that means "ask for the reset" instead of a Keep / Revert apply */

#endif
