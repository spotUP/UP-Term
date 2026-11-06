/* gthemes_core: the Omarchy colour themes of the drawn Amarchy menu (plan 2026-10-26-amarchy-gui-themes.md). A theme is
 * one file NAME.theme holding an Omarchy colors.toml verbatim (`key = "#rrggbb"`, `mode = "dark"`), read by the upconf
 * reader (quotes and `#` are tolerated by upconf_hex). Six keys drive the menu: background, foreground,
 * dark_foreground, accent, green, red; the other keys of the file are kept and ignored, so a copied Omarchy file works. The
 * shipped drawer is UP-Term:Amarchy/Themes, the user's ENVARC:Amarchy/themes; a user file of the same name wins.
 * No OS calls: the file read comes in as a callback (the DOS side is gthemes_dos.c). */
#ifndef GTHEMES_CORE_H
#define GTHEMES_CORE_H

#include "../config/upconf.h"
#include "../prefs/prefs_core.h"

#define GT_NAME 40
#define GT_MAX 64
#define GT_TITLE 64
#define GT_SHIPPED_DIR "UP-Term:Amarchy/Themes"
#define GT_USER_DIR "ENVARC:Amarchy/themes"
#define GT_EXT ".theme"
#define GT_DEFAULT "tokyo-night"

typedef struct gtheme {
    char name[GT_NAME];
    uc_u32 bg, fg, dim, accent; /* 0xRRGGBB: background, foreground, dark_foreground, accent */
    uc_u32 ok, fail;            /* the job view's Done and Failed footers: green and red; no key = accent, foreground */
    int light;                  /* mode = "light" */
    /* desktop theme (plan 2026-10-29-amarchy-p1a-native-theme.md, R1.5): the keys of the file that tint the Workbench.
     * A missing key is derived (OD5): dark = bg; darker = dark_background, else bg at 80 % brightness; lighter = bg
     * moved a tenth toward fg; selection = lighter; muted = dim. */
    uc_u32 dark_bg, darker_bg, lighter_bg, selection, muted;
    uc_u32 inactive;        /* inactive= : the dragbar, gadget boxes, ring, arrows and knob of an inactive window (S32-INACTIVE-BAR); derived: gt_mix(bg, fg, 45) */
    char backdrop[GT_NAME]; /* backdrop= : the drawer name of the theme's backdrops, "" = the theme name */
    char font[GT_NAME];     /* font= : a font file name such as helvetica.font, "" = keep the user's font */
    char wbtitle[GT_TITLE]; /* wbtitle= : a Workbench title format; carried, the title stays stock (OD11) */
    struct gt_model *model; /* TE-1.6: the theme editor's model; 0 until gt_model_attach / gt_load_model; freed by gt_free */
} gtheme;

/* RL8 FLAT-COLOUR-OVERHAUL: THE role table. Every Amarchy UI colour is one of these six roles of a gtheme, made by the one
 * derivation gt_roles_of, the same rule for every theme; the desktop pens (DK_*), the menu roles (MT_*), the icon slots (IP_*) and
 * the flat glyph roles (FLAT_ROLE_*) are views of it, never their own picks from the gtheme.
 *   GR_BACKGROUND  the desktop backdrop, the darkest tone (gt_darker)
 *   GR_SURFACE     windows, panels, the menu and the bar ground (background)
 *   GR_TEXT        text and active glyphs (foreground)
 *   GR_MUTED       secondary text: file names, dim labels, the menu's dim header (muted; dark_foreground when the file has none)
 *   GR_ACCENT      the one highlight: selected row box, border, outline, header (accent)
 *   GR_SELECTION   raised fills: buttons, scroller knob (selection, pushed off the surface by gt_selection)
 *   GR_INACTIVE    S32-INACTIVE-BAR: the bar and frame tone of an inactive window (inactive; background moved GT_INACTIVE_PCT toward the foreground when the file has none)
 * ok and fail are not UI roles: the two status tones of the job view's Done / Failed footers (green, red). */
enum { GR_BACKGROUND, GR_SURFACE, GR_TEXT, GR_MUTED, GR_ACCENT, GR_SELECTION, GR_INACTIVE, GR_ROLES };
#define GT_INACTIVE_PCT 45
/* a moved pct percent of the way toward b, per channel (0xRRGGBB) */
uc_u32 gt_mix(uc_u32 a, uc_u32 b, int pct);
typedef struct gt_roles {
    uc_u32 c[GR_ROLES]; /* 0xRRGGBB per GR_* */
    uc_u32 dim;         /* RL8 step 3: GR_MUTED pushed off the surface to GT_FLOOR_MUTED; the menu's dim item (MT_DIM), the readable disabled tone */
    uc_u32 ok, fail;    /* status tones */
} gt_roles;
void gt_roles_of(const gtheme *t, gt_roles *r);

/* the role pens and desk_role live here, not in desk_core, because the theme editor (gt_pen_budget) needs them and amarchy-index
 * links this file without desk_core.c; desk_core.h includes this header so every desktop user still sees them */
/* the eight palette roles of the flat mapping (OD13): the pen index of a role is its enum value.
 * FILENAME-PEN: Workbench draws the list-view file names with the fixed DrawInfo pen 1, so pen 1 holds the muted text colour
 * (DK_MUTED) and follows every theme; Amarchy's own dark tone (DK_DARKER: BLOCKPEN, BARTRIMPEN, the label backdrop) lives on pen 5.
 * RL8: a view of the role table (gt_roles_of): BG = GR_SURFACE, MUTED = GR_MUTED, FG = GR_TEXT, ACCENT = GR_ACCENT, SELECTION =
 * GR_SELECTION, DARKER = GR_BACKGROUND, GREEN = status ok. DK_BACKDROP (pen 7, was DK_RED) is the Workbench backdrop pen at 3+
 * planes; palette.prefs still writes the fail tone there (byte-compatible), the screen gets desk_pen_colour's backdrop colour. */
enum { DK_BG, DK_MUTED, DK_FG, DK_ACCENT, DK_SELECTION, DK_DARKER, DK_GREEN, DK_BACKDROP, DK_ROLES };
uc_u32 desk_role(const gtheme *t, int role);
/* the role pen (DK_BG..DK_DARKER; DK_ACCENT at most on depth <= 2) nearest to rgb 0xRRGGBB at a screen depth: squared RGB distance,
 * ties to the lower pen. The one rule behind ip_fold and the pen budget. */
int gt_nearest_role_pen(const gtheme *t, uc_u32 rgb, int depth);

/* the colour maths of the derivation (moved from desk_core, R1.18a): brightness 0..255, a contrast ratio times 100 (100 = equal) */
int gt_luma(uc_u32 rgb);
int gt_contrast(uc_u32 a, uc_u32 b);
#define GT_FLOOR_TEXT 220 /* text on a ground: measured, the weakest shipped pair is rose-pine at 233 */
#define GT_STEP_DARK 108  /* the darker tone against the background */
#define GT_FLOOR_SEL 150  /* the selection (button fill, scroller knob) against the surface: shipped light themes sit at 110-130 */
#define GT_FLOOR_MUTED 220 /* RL8 step 3: the muted tone (menu dim item, disabled label) against the surface; same floor as text, shipped themes sat at 111-579 */
#define GT_FLOOR_DIM 150 /* TE-8 P1: text dimmed on purpose (the inactive window title) against its ground; the default value is pushed to it */
#define GT_BLACK_LIFT 0x222222UL /* the backdrop over a black ground: dark grey, lighter than black */
/* 1 when the ground is black on a 12-bit screen: all channels below 16 */
int gt_ground_black(const gtheme *t);
/* GR_BACKGROUND (OD5, DESK-BACKDROP-DARKER): the theme's darker_background when it is darker than the background and keeps its
 * distance, else the background at 80 % or less, in every case strictly darker after the 12-bit nibble cut so the backdrop pen never
 * equals the ground pen. A ground that is black on the screen keeps its colour and the backdrop is GT_BLACK_LIFT. */
uc_u32 gt_darker(const gtheme *t);
/* GR_SELECTION: t->selection pushed darker (light ground) or lighter (dark ground) in 10 % steps until its contrast against the
 * surface is >= GT_FLOOR_SEL, on the 24-bit and on the 12-bit screen */
uc_u32 gt_selection(const gtheme *t);
uc_u32 gt_muted(const gtheme *t);

/* The compiled-in theme (tokyo-night, the default): a bare install without the data drawer still draws. */
void gt_builtin(gtheme *out);

/* Parse the text of a colors.toml (len bytes) into out; a missing or unreadable key keeps the built-in value. */
void gt_parse(const char *buf, long len, gtheme *out);

/* The terminal colours of a theme (Omarchy recolours every terminal with the theme): the text of one UP-Term profile section
 * (fg, bg, cursor-color, the 16 palette entries) made from the colors.toml text, ready for the console's theme_apply. Returns
 * the length, 0 when it does not fit cap. */
int gt_term_text(const char *buf, long len, const char *name, char *out, int cap);

/* The file the console handler's tabs watch: a written theme recolours every open tab (vtcon_handler.c watch_worker). */
#define GT_TERM_FILE "ENV:up-term/theme-active"

/* Every channel of an 0xRRGGBB scaled to pct percent (0..100); the one brightness scaler of the desktop theme. */
uc_u32 gt_scale(uc_u32 rgb, int pct);

/* name -> the file path in a drawer ("dir/name.theme"; a dir ending in ':' or '/' gets no extra slash). 0 when too long. */
int gt_path(const char *dir, const char *name, char *out, int cap);

/* "tokyo-night.theme" -> "tokyo-night" (extension compared without case); 0 when the file is not a theme file. */
int gt_name_of_file(const char *file, char *out, int cap);

typedef struct gt_list {
    int n;
    char name[GT_MAX][GT_NAME];
    char user[GT_MAX]; /* 1: the user's drawer holds this name (it wins over the shipped file) */
} gt_list;

/* Add one file name found in a drawer. Sorted by name without case, one row per name, the user's file marks the row. */
void gt_list_add(gt_list *l, const char *file, int from_user);

typedef long (*gt_read_fn)(void *ctx, const char *path, char *buf, long cap); /* length, -1 missing */

/* Load a theme by name: the user's file, else the shipped one, else (unknown or unreadable name) the built-in default. */
void gt_load(gt_read_fn rd, void *ctx, const char *name, gtheme *out);

/* gt_term_text of the theme NAME as gt_load finds it (the user's file, else the shipped one); 0 when there is no such theme. */
int gt_load_term(gt_read_fn rd, void *ctx, const char *name, char *out, int cap);

/* One drawer's file names, given one by one to emit (the DOS side walks the drawer; a host test fakes it). */
typedef void (*gt_emit_fn)(void *ectx, const char *file);
typedef void (*gt_dir_fn)(void *ctx, const char *dir, gt_emit_fn emit, void *ectx);

/* The theme list of both drawers: shipped first, then the user's (whose file wins and marks the row). */
void gt_scan(gt_dir_fn dir, void *ctx, gt_list *l);

/* The menu row label: the name, with " (current)" appended when it is the saved theme (compared without case). */
void gt_label(const char *name, const char *current, char *out, int cap);

/* 1 when name can be saved as the current theme: non-empty, fits GT_NAME, no path characters ('/' or ':'). */
int gt_name_ok(const char *name);

/* F3-THEME-BOOT: the startup write of GT_TERM_FILE. Calls send(name) once with the configured theme (AM_GTHEME) or GT_DEFAULT when
 * it is empty or not a usable name; returns how many times send ran (1). */
int gt_boot_term(const char *configured, void (*send)(const char *name));

/* A theme row's path in the menu index ("gtheme:<name>"); gt_path_name gives the name back, 0 when the path is not one. */
#define GT_ROW_PREFIX "gtheme:"
int gt_path_name(const char *path, char *out, int cap);

/* The theme the open menu draws while the cursor walks Style > Theme (owner, 2026-10-28: preview on cursor move):
 * the theme of the selected row (row_path "gtheme:<name>"), else the saved one (current), so leaving the list or
 * moving onto another row restores it. Returns 1 and writes the name to out when it differs from drawn (compared
 * without case), 0 when the menu already draws it. row_path may be 0. */
int gt_preview_pick(const char *row_path, const char *current, const char *drawn, char *out, int cap);

/* THEME-PREVIEW-SHELL (owner, 2026-10-29): the picker's preview also recolours the open consoles and agent tabs. gt_preview_pick, and
 * when it picks a theme send(name) runs (the bridge that Apply uses: gt_dos_term_send, GT_TERM_FILE) on every highlight move, no
 * throttle; leaving the list or cancelling picks the saved theme, so send restores it. Returns what gt_preview_pick returned. */
int gt_preview_term(const char *row_path, const char *current, const char *drawn, char *out, int cap,
                    void (*send)(const char *name));

/* THEME-PREVIEW-LIVE-A1200: a picker that keeps no pens of its own (the text menu amarchy-menu, drawn in the console it
 * recolours) holds only drawn, the theme the consoles show now. gt_preview_term with drawn as both the compare and the
 * output: the highlight's theme, or current on any other row / at close (Esc restores the theme active before the picker
 * opened). Start drawn as current when the menu opens, so opening it writes nothing. Returns what gt_preview_term did. */
int gt_preview_step(const char *row_path, const char *current, char *drawn, int cap, void (*send)(const char *name));


/* Why a theme save failed, as the text the menu shows. step: GT_SAVE_NAME (the name check), or a prefs_save_at result
 * (PREFS_SAVE_NODIR, PREFS_INSTALL_WRITE / BACKUP / PLACE / STRANDED); failed: the PREFS_T_ target; err: the DOS error
 * (0: unknown). Writes e.g. "the theme could not be saved: write ENV:Amarchy/prefs, error 202". */
#define GT_SAVE_NAME (-200)
/* TE-3 refusals, the same text path: the name is a shipped theme's (D2: a shipped theme is never shadowed), the name is taken,
 * no such user theme, a write failed. (A file that is no theme gets the message of gt_check_text.) */
#define GT_SAVE_SHIPPED (-201)
#define GT_SAVE_EXISTS (-202)
#define GT_SAVE_MISSING (-203)
#define GT_SAVE_WRITE (-205)
void gt_save_text(int step, int failed, long err, char *out, int cap);

/* ---- Theme file format v2 and the model (plan 2026-11-05-amarchy-theme-editor.md, TE-1) ----
 * A v2 file keeps every v1 key in the default section (written from the resolved groups, so an older Amarchy reads it as v1)
 * and adds the sections [profile swatches|groups|elements|look]. The model is what the editor edits; the role views read it.
 * A file without `amarchy-theme = 2` is v1: gt_model_from_v1 builds the model that reproduces gt_roles_of byte for byte. */
#define GT_SWATCH_MAX 32
#define GT_SWATCH_NAME 16
#define GT_STATES 4
#define GT_REF_DEPTH 4
#define GT_NESTED_MAX 192 /* nested ref slots (plan said 64: ten groups with a mix in three states already need 60) */
#define GT_EXTRA_MAX 1536 /* kept lines: Omarchy keys the v1 reader ignores (the terminal palette reads them) and unknown v2 keys */
enum { GT_ST_NORMAL, GT_ST_HOVER, GT_ST_PRESSED, GT_ST_DISABLED };
enum { GT_REF_NONE, GT_REF_SWATCH, GT_REF_RGB, GT_REF_DARKER, GT_REF_SELECTION, GT_REF_MUTED, GT_REF_MIX, GT_REF_GROUP };
enum { GT_WARN_CYCLE = 1, GT_WARN_OVERFLOW = 2, GT_WARN_UNKNOWN_KEY = 4, GT_WARN_BAD_REF = 8 };
enum { GT_FONT_SCREEN, GT_FONT_TITLE, GT_FONT_MENU, GT_FONT_ICON, GT_FONT_SHELL, GT_FONTS };

/* the groups: one per current role, so v1 maps 1:1 */
enum { GTG_BACKGROUND, GTG_SURFACE, GTG_TEXT, GTG_MUTED_TEXT, GTG_ACCENT, GTG_SELECTION, GTG_INACTIVE, GTG_DIM, GTG_SUCCESS,
       GTG_FAILURE, GTG_COUNT };
/* the 36 elements (owner answer D5: all of them), the order of gt_elem_defs[] */
enum { GTE_TITLE_BAR, GTE_TITLE_BAR_INACTIVE, GTE_TITLE_TEXT, GTE_TITLE_TEXT_INACTIVE, GTE_BORDER, GTE_BORDER_INACTIVE,
       GTE_GADGET_GLYPH, GTE_GADGET_GLYPH_INACTIVE, GTE_SCROLL_TRACK, GTE_SCROLL_KNOB, GTE_MENU_BACKGROUND, GTE_MENU_TEXT,
       GTE_MENU_SELECTION, GTE_MENU_SELECTED_TEXT, GTE_MENU_HEADER, GTE_MENU_SEPARATOR, GTE_REQUESTER_BACKGROUND,
       GTE_BUTTON_FACE, GTE_BUTTON_TEXT, GTE_TEXT_FIELD, GTE_TEXT_CURSOR, GTE_DESKTOP_BACKDROP, GTE_ICON_LABEL_TEXT,
       GTE_ICON_LABEL_SHADOW, GTE_ICON_LABEL_OUTLINE, GTE_ICON_FOREGROUND, GTE_ICON_ACCENT, GTE_ICON_MUTED,
       GTE_ICON_SELECTION_PLATE, GTE_HELPER_BACKGROUND, GTE_HELPER_TEXT, GTE_TERMINAL_BACKGROUND, GTE_TERMINAL_FOREGROUND,
       GTE_TERMINAL_CURSOR, GTE_SUCCESS_TEXT, GTE_FAILURE_TEXT, GTE_COUNT };

typedef struct gt_ref {         /* one colour reference */
    unsigned char kind;         /* GT_REF_* */
    unsigned char a, b, pct;    /* SWATCH: a = swatch; GROUP: a = group; DARKER/SELECTION/MUTED: a = nested slot; MIX: a, b = nested slots, pct */
    uc_u32 rgb;                 /* kind RGB */
} gt_ref;

typedef struct gt_look {        /* type, shape, states, desktop */
    char font[GT_FONTS][40];    /* "name.font/size" */
    unsigned char title_align, title_style;
    unsigned char corner_radius, border_width, bar_height, gadget_size, gadget_pad, scroll_width;
    unsigned char knob_min, knob_shape, glyph_set, gadget_order;
    unsigned char ghost_style, hover_on, inactive_pct;
    uc_u32 pointer[3];
} gt_look;

typedef struct gt_model {
    int version;                /* 1 or 2 as read */
    int nswatch;
    int nnested;
    int light;                  /* mode = "light" */
    char name[GT_NAME];         /* name= key, "" when absent */
    char swatch_name[GT_SWATCH_MAX][GT_SWATCH_NAME];
    uc_u32 swatch[GT_SWATCH_MAX];
    gt_ref group[GTG_COUNT][GT_STATES];
    gt_ref elem[GTE_COUNT][GT_STATES]; /* kind NONE = follows its group */
    gt_ref nested[GT_NESTED_MAX];
    gt_look look;
    char base[GT_NAME];         /* the theme this one was duplicated from */
    char backdrop[GT_NAME];     /* the v1 backdrop=, font=, wbtitle= keys, carried */
    char font[GT_NAME];
    char wbtitle[GT_TITLE];
    unsigned warn;              /* GT_WARN_* */
    int extralen;
    char extra[GT_EXTRA_MAX];   /* kept lines "<section letter><key> = <value>\n": D default, S swatches, G groups, E elements, L look */
} gt_model;

typedef struct gt_group_def {
    const char *key, *label, *def; /* def: the v1 ref text, over the swatch names of gt_model_from_v1 */
} gt_group_def;
typedef struct gt_elem_def {
    const char *key, *label;
    unsigned char group;        /* default group */
    unsigned char states;       /* bit per GT_ST_* the element uses */
    const char *consumer;       /* menu role, desk pen, DrawInfo slot, flat role, icon slot or terminal */
} gt_elem_def;
extern const gt_group_def gt_group_defs[GTG_COUNT];
extern const gt_elem_def gt_elem_defs[GTE_COUNT];

/* One colour reference from text (grammar of the plan, section 2.2). 1 and *out set; 0 and warn bits on a bad text (unknown
 * swatch or group, depth over GT_REF_DEPTH, pool full). Empty text gives kind NONE and 1. */
int gt_ref_parse(gt_model *m, const char *text, gt_ref *out);
/* The text of a ref into out: its length, -1 when it does not fit cap. */
int gt_ref_text(const gt_model *m, const gt_ref *r, char *out, int cap);
/* A ref's colour for a state (group refs read that group's same state; a cycle was cut at load). */
uc_u32 gt_ref_rgb(const gt_model *m, const gt_ref *r, int state);

/* The element's own ref if set, else its group's ref of that state, else the group's normal state. */
gt_ref gt_resolve(const gt_model *m, int elem, int state);
uc_u32 gt_resolve_rgb(const gt_model *m, int elem, int state);
/* Group colour of a state (an unset state falls back to normal). */
uc_u32 gt_group_rgb(const gt_model *m, int group, int state);

/* v1 gtheme -> model: one swatch per v1 colour, every group at its v1 ref. */
void gt_model_from_v1(const gtheme *t, gt_model *m);
/* Parse theme text of either version into m (v1 text goes through gt_parse and gt_model_from_v1). */
void gt_model_parse(const char *buf, long len, gt_model *m);
/* Every GR_* role is the resolved normal state of its group: equals gt_roles_of for a v1 file. */
void gt_roles_from_model(const gt_model *m, gt_roles *r);
/* The v1 keys of a model (what an older Amarchy reads), colours resolved from the groups. */
void gt_model_to_v1(const gt_model *m, gtheme *out);
/* TE-7-FONT-MAP: the font of slot which (GT_FONT_*) a desk writer applies: the model's look.font "name.font/size" (name and size split;
 * size 0 when absent), for GT_FONT_ICON the v1 font= key when the model has none. 1 when there is a name, 0 when the theme keeps the user's. */
int gt_look_font(const gtheme *t, int which, char *name, int cap, int *size);
/* the model's title bar height in pixels, 0 = stock / no model */
int gt_look_bar_height(const gtheme *t);
/* The file text: v1 default keys from the resolved groups, then the v2 sections. Its length; -1 when it does not fit cap or a
 * value would pass UC_MAX_VALUE. */
long gt_model_save_text(const gt_model *m, char *out, long cap);

/* TE-2.2: a role view's element reader. The colour of element elem (normal state) when the theme's model gives that element its own
 * ref, else fallback (the group value the view derived). A theme without a model, and an element left to follow its group, always
 * give fallback, so a view's output with no override stays byte-equal to the group derivation. */
uc_u32 gt_elem_rgb(const gtheme *t, int elem, uc_u32 fallback);

/* TE-1.6: give t a model parsed from text (heap, freed by gt_free), and gt_load that attaches it. gt_load itself never
 * allocates: callers copy gtheme by value. */
int gt_model_attach(gtheme *t, const char *buf, long len);
void gt_load_model(gt_read_fn rd, void *ctx, const char *name, gtheme *out);
void gt_free(gtheme *t);

/* ---- TE-3: Save, Save As, Duplicate, Import, Export, Delete (plan TE-3; the DOS side is gthemes_dos.h) ----
 * All of it runs over a prefs_fs, so a host test drives it on a memory file system. The user file is GT_USER_DIR/<name>.theme with
 * the copy in GT_ENV_DIR; the drawers must exist (gthemes_dos makes them). D2: an edited shipped theme is saved under a new name,
 * never over the shipped file and never as a same-name user copy. */
#define GT_ENV_DIR "ENV:Amarchy/themes"
/* The editor's proposed name for a copy of base: "<base> Custom", then "<base> Custom 2" and so on until taken(ctx, name) is 0.
 * Cut to fit GT_NAME. taken may be 0 (never taken). */
void gt_custom_name(const char *base, int (*taken)(void *ctx, const char *name), void *ctx, char *out, int cap);
/* 1 when the theme text parses as a theme; else 0 and msg says why (empty, no keys, a bad reference, a group cycle, too many
 * references). msg may be 0. */
int gt_check_text(const char *buf, long len, char *msg, int cap);
/* Save the model as the user theme name (both files). 1 saved; 0 with why filled (gt_save_text). Refused: a bad name, a shipped
 * theme's name that has no user file (D2). */
int gt_save(const prefs_fs *fs, const char *name, const gt_model *m, char *why, int cap);
/* Save As: gt_save under a new name; refused (GT_SAVE_EXISTS) when the user or the shipped drawer holds it. */
int gt_save_as(const prefs_fs *fs, const char *name, const gt_model *m, char *why, int cap);
/* Duplicate: theme from (the user's file, else the shipped one) written as the new user theme to (a v2 file). */
int gt_duplicate(const prefs_fs *fs, gt_read_fn rd, void *ctx, const char *from, const char *to, char *why, int cap);
/* Import: the file at src (read through rd) is checked by gt_check_text and copied byte for byte as the user theme name.
 * Export: theme name copied to the path dst. Both 0 with why filled on a refusal. */
int gt_import(const prefs_fs *fs, gt_read_fn rd, void *ctx, const char *src, const char *name, char *why, int cap);
int gt_export(const prefs_fs *fs, gt_read_fn rd, void *ctx, const char *name, const char *dst, char *why, int cap);
/* Delete User Theme: the user file and its ENV: copy go, the shipped file of that name (if any) is untouched and shows again.
 * GT_SAVE_MISSING when the user holds no such file. */
int gt_delete_user(const prefs_fs *fs, const char *name, char *why, int cap);
/* TE-3.2: save the edited theme, and when it is the current one (name compared without case) call apply(name): the file changed
 * under the same name, so the drawn desktop must be re-applied. The sentinel counts every apply call. 1 saved. */
extern unsigned long gt_dos_edit_applies;
int gt_save_edit(const prefs_fs *fs, const char *name, const gt_model *m, const char *current, int (*apply)(const char *name),
                 char *why, int cap);
/* TE-7.1 (D1): the editor's Save. gt_save_edit, then, when the saved theme is the current one and bound(running, saved) is not 0,
 * the reset-bound fields changed (wbr_bound_mask): ask(mask) shows the "Reset Workbench Now / Later" prompt. The running model is
 * the file as loaded or last saved (gt_ed.saved) when the buffer holds this theme. Sentinel gt_dos_reset_prompts counts the asks. */
extern unsigned long gt_dos_reset_prompts;
int gt_save_editor(const prefs_fs *fs, const char *name, const gt_model *m, const char *current, int (*apply)(const char *name),
                   unsigned (*bound)(const gt_model *running, const gt_model *neu), void (*ask)(unsigned mask), char *why, int cap);

struct sd_setting; /* settings_def.h includes this header through prefs_ui.h: the rows are named by tag */

/* ---- TE-4: the editor's edit buffer and its rows (plan sections 4 and 6) ----
 * The Theme Editor pages edit ONE gt_model copy (gt_ed.m); gt_ed.saved is the file as loaded or last saved, so Revert is a copy back
 * and "unsaved changes" is a compare. The rows are sd_setting entries (kinds SD_SWATCH, SD_SWATCHREF, SD_ELEMENT, SD_GACTION and
 * store SD_GTHEME) numbered SDI_COUNT and up: gte_row(id) is the one lookup for any row id, sd_settings for the old ones. */
enum { GTR_OK = 0, GTR_BAD = -1, GTR_IN_USE = -2, GTR_FULL = -3, GTR_EXISTS = -4, GTR_NAME = -5 };
typedef struct gt_edit {
    gt_model m, saved;
    int open;                   /* a theme is loaded into the buffer */
    int advanced;               /* Mode: Advanced (the Elements page is shown) */
    int depth;                  /* TE-8.3: bitplanes of the screen the editor runs on (EHB counts as 5; 15 and up = truecolour); 0 = unknown */
    char name[GT_NAME];         /* the theme being edited */
} gt_edit;
extern gt_edit gt_ed;
extern unsigned long gte_edits; /* sentinel: every successful change of the buffer */
/* TE-6.3 (D3): live on every edit. gte_live is called with the buffer after each change and after a revert; the DOS side sets it
 * (gt_dos_edit_open) to the model preview. gte_live_sends counts the calls (the reachability sentinel). */
extern void (*gte_live)(const gt_model *m);
extern unsigned long gte_live_sends;
void gte_open(const gt_model *m, const char *name);      /* load the buffer; Mode is Advanced when the file has an override */
int gte_dirty(void);                                      /* the buffer differs from the file */
void gte_revert(void);                                    /* reload the file's model */
void gte_saved(void);                                     /* the buffer is now the file (after a save) */
int gte_swatch_count(void);
int gte_swatch_users(int sw, char *out, int cap);         /* refs that use swatch sw (groups, elements, nested); names in out */
int gte_swatch_set(int sw, const char *text);             /* "#rrggbb" */
int gte_swatch_add(const char *name, uc_u32 rgb);
int gte_swatch_find(const char *name);                   /* the swatch of that name (case blind), -1 when none */
int gte_swatch_rename_name(const char *old, const char *name); /* GTR_BAD when old names no swatch */
int gte_swatch_remove_name(const char *old, char *why, int cap);
int gte_swatch_rename(int sw, const char *name);
int gte_swatch_remove(int sw, char *why, int cap);        /* GTR_IN_USE while used, with the users named in why */
int gte_group_set(int group, int state, const char *text);
int gte_elem_set(int elem, int state, const char *text);  /* empty text clears */
void gte_elem_clear(int elem);                            /* all states follow the group again */
int gte_elem_overridden(int elem);
int gte_group_overridden(int group);                      /* some element of the group has its own colour */
int gte_ref_step(const gt_ref *cur, int dir, char *out, int cap); /* the next swatch name after cur (Darker/Selection/Muted forms after the swatches) */
/* the row's text for the menu, and the setting of it */
const struct sd_setting *gte_row(int id);
int gte_nrows(void);                                      /* rows numbered SDI_COUNT .. SDI_COUNT + gte_nrows() - 1 */
int gte_row_visible(const struct sd_setting *s);                 /* a swatch past the count, an element state it does not use, or an Elements page in Simple mode are not */
int gte_row_text(const struct sd_setting *s, char *out, int n);  /* current value as text */
int gte_row_set_text(const struct sd_setting *s, const char *text); /* GTR_OK or GTR_BAD, GTR_* */
int gte_row_step(const struct sd_setting *s);
/* A cycle row (Mode, the look cycles) to value index. */
int gte_row_set_index(const struct sd_setting *s, int index);                    /* cycle rows, ref rows (next swatch) */
int gte_row_label(const struct sd_setting *s, char *out, int n); /* "Label: value" */
enum { GTA_OPEN = 1, GTA_SAVE, GTA_SAVE_AS, GTA_REVERT, GTA_APPLY, GTA_ADD_SWATCH, GTA_RENAME_SWATCH, GTA_REMOVE_SWATCH,
       GTA_CLEAR_ELEMENT, GTA_DUPLICATE, GTA_IMPORT, GTA_EXPORT, GTA_RESET_BASE, GTA_DELETE,
       GTA_SCR_ALL, GTA_SCR_NONE, GTA_SCR_INVERT, GTA_SCR_ADD, GTA_SCR_SAVE, GTA_SCR_UP, GTA_SCR_DOWN, GTA_SCR_REMOVE };
/* What an editor action does before it touches a file (the menu's theme_action follows this, the host tests assert it):
 * GTP_SKIP nothing to act on (no theme open), GTP_LEAVE ask Save/Discard/Cancel first (the buffer is replaced),
 * GTP_NAME ask for a name. */
enum { GTP_SKIP = 1, GTP_LEAVE = 2, GTP_NAME = 4 };
int gte_action_plan(int aux, int editor_open);
/* gt_dos_edit_open's decision: read the named theme through rd and open its model in the buffer. 1 opened; 0 when it has none. */
int gte_open_from(gt_read_fn rd, void *ctx, const char *name);

/* ---- TE-8: presets, contrast warning, pen budget (plan 2026-11-05-amarchy-theme-editor.md sections 6, 7, D8) ---- */
/* TE-8.1: a theme as a starting point: the user file, else the shipped file, else (the name is GT_DEFAULT) the built-in tokyo-night.
 * Parsed into out with out->name = name. 1 found; 0 when no such theme. */
int gt_preset_model(gt_read_fn rd, void *ctx, const char *name, gt_model *out);
/* Reset to Base: the edit buffer takes the model of its base theme (gt_preset_model of gt_ed.m.base), keeps its own name and base,
 * stays dirty against the file and goes live like any edit. 1 reset; 0 when no editor is open, no base is named or it is gone. */
int gte_reset_to_base(gt_read_fn rd, void *ctx);

/* TE-8.2: one text-on-ground pair. kind is the floor it is held to. below = measured contrast under the floor (D8: shown as a
 * warning on the row and in the summary; Save stays allowed). */
enum { GTC_TEXT, GTC_MUTED, GTC_SEL, GTC_DIM };
typedef struct gt_cpair {
    unsigned char text, text_state, ground, ground_state, kind; /* GTE_* / GT_ST_* of the two sides, GTC_* floor */
    int contrast, floor, below;
} gt_cpair;
#define GT_CPAIRS 16
/* Every pair of the table measured on the resolved colours (gt_contrast against GT_FLOOR_TEXT / GT_FLOOR_MUTED / GT_FLOOR_SEL / GT_FLOOR_DIM).
 * Fills out (room for GT_CPAIRS); returns the pair count. */
int gt_contrast_pairs(const gt_model *m, gt_cpair out[GT_CPAIRS]);
/* the number of pairs below their floor; when first >= 0, *first is the index of the first such pair (else -1) */
int gt_contrast_warnings(const gt_model *m, int *first);
/* 1 when element elem is the text side of a pair below its floor (the row warning of the Elements page) */
int gt_contrast_elem_warns(const gt_model *m, int elem);

/* TE-8.3: the pen budget of a model at a screen depth (bitplanes; EHB counts as 5; >= 15 truecolour). Pure: it reads the model and
 * the depth, never obtains a pen.
 *   exact     truecolour: colours are drawn directly, no shortage.
 *   pens      the pens the screen has (1 << depth, 256 for depth >= 8); held = pens the desktop keeps: depth <= 3 the role pens
 *             (8, 4 on depth 2), depth 4 the 8 role pens and the 8 icon pens (16), depth 5 the same plus pointer 17-19 (19), depth
 *             >= 6 role 8 + pointer 3 + icon 16 (27); free = pens - held.
 *   colours   distinct resolved normal-state colours over the 36 elements;  shared = elements - colours (elements that repeat a colour).
 *   needing   distinct colours no role pen carries (and, from depth 4, no icon element: those use the held icon pens); they take a
 *             free pen or the nearest one. fold = the part of needing that finds no free pen (depth <= 3: all of it).
 *   grp_pen   depth <= 3 only: the role pen each group's normal colour folds to (the ip_fold rule: nearest, ties to the lower pen,
 *             never DK_GREEN / DK_BACKDROP); grp_with[g] = the first group on the same pen (g itself when alone). Else -1 / g. */
typedef struct gt_budget {
    int depth, exact, pens, held, free, colours, shared, needing, fold;
    signed char grp_pen[GTG_COUNT], grp_with[GTG_COUNT];
} gt_budget;
void gt_pen_budget(const gt_model *m, int depth, gt_budget *out);
/* the summary row text, e.g. "Screen 8 colours: 4 elements fold to the nearest pen" / "Truecolour: exact colours" */
void gt_budget_text(const gt_budget *b, char *out, int n);
/* the share line of group g ("Text shares a pen with Surface"); 0 and out empty when g is alone or the screen is not shallow */
int gt_budget_share_text(const gt_budget *b, int g, char *out, int n);

/* TE-10.5a: the pen plan of one public screen. Pure: reads the model, the depth and the masks; never obtains or writes a pen.
 * One entry per colour the budget calls "needing" (gt_pen_budget .needing: distinct colours no role pen carries, no icon element
 * from depth 4), so the count equals that field; truecolour (exact) plans nothing. The colours come from the same walk as the budget.
 *   free_mask  256 bits, pen p is bit (p & 31) of word p >> 5: the pens of the screen's colour map its owner does not use.
 *   exclusive  1: Amarchy writes rgb to pen (SetRGB32). 0 = best: pen is the nearest role pen, nothing is written.
 *   Exclusive pens are only pens >= the budget's held count (the desktop keeps the low ones). In ascending order first the pens in
 *   free_mask, then, only when allow_owner_pens (the rule's Take Over Palette), the owner's pens. A colour that finds none is best.
 *   Each pen is planned exclusive at most once. cap = capacity of out; more entries than cap are cut off (the return value is the
 *   number written, never more than cap). Returns the number of entries. */
typedef struct gt_plan_pen {
    short pen;
    uc_u32 rgb;      /* 0xRRGGBB */
    short exclusive; /* 1 = write the colour to pen; 0 = best match, no write */
} gt_plan_pen;
#define GT_MASK_WORDS 8
int gt_screen_apply_plan(const gt_model *m, int depth, const uc_u32 *free_mask, int allow_owner_pens, gt_plan_pen *out, int cap);


/* TE-10.2: the screen rules, ENVARC:Amarchy/screen-themes (ENV: copy). Line 1 "default all|workbench|none", then one rule per line,
 * in order: "<pattern> = <theme>|exclude [takeover]". Pure: the text comes in and goes out as a buffer. */
#define GT_SCR_MAX 32    /* rules kept */
#define GT_SCR_PAT 140   /* a screen name (139) or pattern, with the terminator */
#define GT_SCR_ALL 0
#define GT_SCR_WORKBENCH 1
#define GT_SCR_NONE 2
typedef struct gt_screen_rule {
    char pat[GT_SCR_PAT];  /* AmigaDOS pattern (#? ? #c), compared without case */
    char theme[GT_NAME];   /* theme name; empty when exclude */
    int exclude;           /* 1 = leave the screen untouched */
    int takeover;          /* Take Over Palette */
} gt_screen_rule;
typedef struct gt_screen_rules {
    int enabled;           /* the on/off key screen-themes (AM_SCREENTHEMES); 0 = today's behaviour */
    int def;               /* GT_SCR_* : what a screen with no matching rule gets */
    int n;
    gt_screen_rule rule[GT_SCR_MAX];
} gt_screen_rules;
/* No text (len 0) gives default workbench and no rules; enabled is set to 1. A theme rule on the exact name Workbench is dropped
 * (the main theme key AM_GTHEME holds it); an exclude rule on it is kept. Bad lines are skipped. */
void gt_screen_rules_parse(const char *buf, long len, gt_screen_rules *out);
/* The file text; exact-name rules first, pattern rules after, each group in its order. Returns the length, -1 when cap is too small. */
long gt_screen_rules_write(const gt_screen_rules *r, char *buf, long cap);
/* Append a rule ("exclude" as theme = Exclude); a theme rule on exact Workbench is accepted in memory, not written. 1 ok, 0 full. */
int gt_screen_rules_add(gt_screen_rules *r, const char *pat, const char *theme, int takeover);
/* Remove the first rule whose pattern equals pat (no case); 1 when one went. */
int gt_screen_rules_remove(gt_screen_rules *r, const char *pat);
/* 1 when pat has no wildcard (an exact name, as a tick in the screen list writes it). */
int gt_screen_exact(const char *pat);
/* AmigaDOS pattern match without case: #? any run, ? one char, #c any run of c. 1 on match. */
int gt_screen_match(const char *pat, const char *name);
extern unsigned long gt_screen_match_steps; /* work counter: one per matcher step (bound test sentinel) */
/* The theme for a public screen name. Returns 1 and fills theme (cap bytes; an unusable name becomes GT_DEFAULT) and *takeover,
 * or 0 when the screen stays untouched. Key off: only Workbench, with main_theme. */
int gt_screen_theme_of(const gt_screen_rules *r, const char *name, const char *main_theme, char *theme, int cap, int *takeover);
/* TE-10.7: the theme a window Amarchy draws itself (the drawn menu, the theme windows) takes on the public screen screen_name:
 * the theme the screen rules give it, else main_theme (an excluded or untouched screen, a screen with no public name: 0 or empty).
 * out is always NUL-terminated within cap (cap > 0). */
void gt_screen_draw_theme(const gt_screen_rules *r, const char *screen_name, const char *main_theme, char *out, int cap);

/* TE-10.8: the rows of the Theme Editor Screens page, an editing view of the same rule list. The token GT_SCR_MAIN as a rule's theme
 * means "this screen takes the main theme, whatever it is" (a colon can never be in a theme name); parse and write carry it unchanged. */
#define GT_SCR_MAIN ":main"
#define GT_SCR_ROWS 40
typedef struct gt_screen_row {
    char name[GT_SCR_PAT];
    int on;                 /* ticked: the screen is themed */
    int open;               /* 1 = in the open-screen list, 0 = a rule for a screen that is not open */
    char theme[GT_NAME];    /* "" = main theme */
    int takeover;
} gt_screen_row;
/* Workbench first (open = 1), then the names of open in the given order (no second Workbench), then every exact-name rule whose
 * screen is not listed (open = 0), while cap has room. Pattern rules give no row. The rules count as switched on (enabled ignored).
 * Returns the row count. */
int gt_screen_rows(const gt_screen_rules *r, const char *const *open, int nopen, gt_screen_row *out, int cap);
/* Row operations edit r in place with the rules switched on. 1 = done, 0 = not possible, r unchanged. A name that is not exact, empty
 * or too long gives 0; a full rule table gives 0 with r as before. Rules that say nothing (an exact ":main" without takeover that the
 * default or a pattern gives anyway) are not kept. */
int gt_screen_row_tick(gt_screen_rules *r, const char *name, int on);
/* theme "" or GT_SCR_MAIN = main theme. Replaces an exact exclude rule (the row becomes ticked), keeps the takeover flag. Workbench: 0. */
int gt_screen_row_theme(gt_screen_rules *r, const char *name, const char *theme);
/* Needs a themed screen (else 0). Without an exact rule one is added with the theme the screen has now. Workbench: 0. */
int gt_screen_row_takeover(gt_screen_rules *r, const char *name, int on);
/* Workbench plus the listed open screens through the tick operation; Default stays. Return the number of screens that could not change. */
int gt_screen_rows_all(gt_screen_rules *r, const char *const *open, int nopen);
int gt_screen_rows_none(gt_screen_rules *r, const char *const *open, int nopen);
int gt_screen_rows_invert(gt_screen_rules *r, const char *const *open, int nopen);

/* TE-10.8b: the pattern rules (the rules whose pattern has a wildcard), numbered p = 0.. in file order. Pure edits of r in place;
 * 1 = done, 0 = not possible and r unchanged. */
int gt_screen_pat_count(const gt_screen_rules *r);
int gt_screen_pat_add(gt_screen_rules *r, const char *pat);            /* theme = main theme, no takeover; the pattern needs a wildcard */
int gt_screen_pat_set(gt_screen_rules *r, int p, const char *pat);
int gt_screen_pat_theme(gt_screen_rules *r, int p, const char *theme); /* "" or GT_SCR_MAIN = main theme, "exclude" = Exclude */
int gt_screen_pat_takeover(gt_screen_rules *r, int p, int on);         /* an exclude has none: 0 */
int gt_screen_pat_move(gt_screen_rules *r, int p, int dir);            /* dir -1 up, +1 down; 0 at the edge */
int gt_screen_pat_remove(gt_screen_rules *r, int p);

/* TE-10.8b: the state behind the Theme Editor Screens page. The page rows (page PG_PAGE_THEME_SCREENS) read and edit this one copy
 * through the gte_row_* functions; the DOS side loads it (gt_se_load), and writes gt_se_text back on Save. */
#define GT_SE_ROWS 24   /* screen rows the page has */
#define GT_SE_PATS 16   /* pattern rules the page has */
enum { GTSE_TICK = 1, GTSE_THEME, GTSE_TAKE, GTSE_DEFAULT, GTSE_PAT, GTSE_PTHEME, GTSE_PTAKE };  /* aux of the page rows; aux2 = index */
extern void (*gt_se_loader)(void);                                     /* DOS glue: called before the first row shows, and again after gt_se_stale, to gt_se_load */
void gt_se_set_themes(const char *const *names, int n);               /* the theme names the theme cycles offer */
void gt_se_load(const char *buf, long len, const char *const *open, int nopen); /* rule file text (len 0 = none) and the open public screens */
long gt_se_text(char *buf, long cap);                                  /* the rule file text, as gt_screen_rules_write */
int gt_se_enabled(void);                                               /* value of the on/off key: 0 only when the rules are today's default */
int gt_se_dirty(void);                                                 /* changed since gt_se_load */
void gt_se_stale(void);                                                /* the open screens may have changed: reload at the next row, unless edits are unsaved */
const gt_screen_rules *gt_se_rules(void);
int gt_se_action(int aux, int index);                                  /* GTA_SCR_ALL .. REMOVE: GTR_OK or GTR_BAD */
int gt_se_add_rule(const char *pat);                                   /* Add Rule: GTR_OK, GTR_BAD (no wildcard) or GTR_FULL */

#endif
