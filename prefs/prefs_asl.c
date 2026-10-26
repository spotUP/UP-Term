/* prefs_asl: the asl.library requesters (see prefs_asl.h). */
#include <string.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <graphics/gfxbase.h>
#include <graphics/displayinfo.h>
#include <proto/graphics.h>

#include "prefs_asl.h"

static void copy_cut(char *dst, const char *src, int cap)
{
    int i = 0;
    while (src[i] && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

int asl_pick_file(struct Screen *scr, struct Window *win, const char *title,
                  const char *drawer, const char *pattern, int reject_icons,
                  char *out, int cap, int *file_named)
{
    struct Library *AslBase = OpenLibrary((STRPTR)"asl.library", 37);
    struct FileRequester *fr;
    struct TagItem t[10];
    int n = 0, ok = 0;
    if (file_named)
        *file_named = 0;
    out[0] = 0;
    if (!AslBase)
        return 0;
    if (win) {
        t[n].ti_Tag = ASLFR_Window;       t[n++].ti_Data = (ULONG)win;
    } else {
        t[n].ti_Tag = ASLFR_Screen;       t[n++].ti_Data = (ULONG)scr;
    }
    t[n].ti_Tag = ASLFR_TitleText;        t[n++].ti_Data = (ULONG)title;
    if (drawer && drawer[0]) {
        t[n].ti_Tag = ASLFR_InitialDrawer; t[n++].ti_Data = (ULONG)drawer;
    }
    if (pattern) {
        t[n].ti_Tag = ASLFR_InitialPattern; t[n++].ti_Data = (ULONG)pattern;
        t[n].ti_Tag = ASLFR_DoPatterns;     t[n++].ti_Data = TRUE;
    }
    t[n].ti_Tag = ASLFR_RejectIcons;      t[n++].ti_Data = (ULONG)(reject_icons != 0);
    t[n].ti_Tag = TAG_DONE;               t[n].ti_Data = 0;
    fr = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, t);
    if (fr && AslRequest(fr, 0)) {
        copy_cut(out, (const char *)fr->fr_Drawer, cap);
        AddPart((STRPTR)out, fr->fr_File, (ULONG)cap);
        if (file_named)
            *file_named = fr->fr_File[0] != 0;
        ok = out[0] != 0;
    }
    if (fr)
        FreeAslRequest(fr);
    CloseLibrary(AslBase);
    return ok;
}

int asl_pick_font(struct Screen *scr, struct Window *win, const char *title,
                  const char *name, long size, int fixed_only,
                  char *out_name, int cap, long *out_size)
{
    struct Library *AslBase = OpenLibrary((STRPTR)"asl.library", 37);
    struct FontRequester *fr;
    struct TagItem t[8];
    int n = 0, ok = 0;
    out_name[0] = 0;
    *out_size = 0;
    if (!AslBase)
        return 0;
    if (win) {
        t[n].ti_Tag = ASLFO_Window;       t[n++].ti_Data = (ULONG)win;
    } else {
        t[n].ti_Tag = ASLFO_Screen;       t[n++].ti_Data = (ULONG)scr;
    }
    t[n].ti_Tag = ASLFO_TitleText;        t[n++].ti_Data = (ULONG)title;
    t[n].ti_Tag = ASLFO_FixedWidthOnly;   t[n++].ti_Data = (ULONG)(fixed_only != 0);
    if (name && name[0]) {
        t[n].ti_Tag = ASLFO_InitialName;  t[n++].ti_Data = (ULONG)name;
    }
    t[n].ti_Tag = ASLFO_InitialSize;      t[n++].ti_Data = (ULONG)(size > 0 ? size : 8);
    t[n].ti_Tag = TAG_DONE;               t[n].ti_Data = 0;
    fr = (struct FontRequester *)AllocAslRequest(ASL_FontRequest, t);
    if (fr && AslRequest(fr, 0) && fr->fo_Attr.ta_Name) {
        copy_cut(out_name, (const char *)fr->fo_Attr.ta_Name, cap);
        *out_size = fr->fo_Attr.ta_YSize;
        ok = out_name[0] != 0;
    }
    if (fr)
        FreeAslRequest(fr);
    CloseLibrary(AslBase);
    return ok;
}

int asl_mode_name(unsigned long id, char *out, int cap)
{
    struct GfxBase *GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 36);
    struct NameInfo ni;
    int ok = 0;
    if (cap < 1)
        return 0;
    out[0] = 0;
    if (!GfxBase)
        return 0;
    if (id && GetDisplayInfoData(0, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, (ULONG)id) > 0 && ni.Name[0]) {
        copy_cut(out, (const char *)ni.Name, cap);
        ok = out[0] != 0;
    }
    CloseLibrary((struct Library *)GfxBase);
    return ok;
}

int asl_pick_screenmode(struct Screen *scr, struct Window *win, const char *title,
                        unsigned long id, int depth, int min_depth, int max_depth,
                        unsigned long *out_id, int *out_depth)
{
    struct Library *AslBase = OpenLibrary((STRPTR)"asl.library", 38);
    struct ScreenModeRequester *sr;
    struct TagItem t[13];
    int n = 0, ok = 0;
    if (!AslBase)
        return 0;
    if (win) {
        t[n].ti_Tag = ASLSM_Window;       t[n++].ti_Data = (ULONG)win;
    } else {
        t[n].ti_Tag = ASLSM_Screen;       t[n++].ti_Data = (ULONG)scr;
    }
    t[n].ti_Tag = ASLSM_TitleText;        t[n++].ti_Data = (ULONG)title;
    t[n].ti_Tag = ASLSM_DoDepth;          t[n++].ti_Data = TRUE;
    t[n].ti_Tag = ASLSM_MinDepth;         t[n++].ti_Data = (ULONG)min_depth;
    t[n].ti_Tag = ASLSM_MaxDepth;         t[n++].ti_Data = (ULONG)max_depth;
    if (id) {
        t[n].ti_Tag = ASLSM_InitialDisplayID; t[n++].ti_Data = (ULONG)id;
    }
    if (depth >= min_depth && depth <= max_depth) {
        t[n].ti_Tag = ASLSM_InitialDisplayDepth; t[n++].ti_Data = (ULONG)depth;
    }
    t[n].ti_Tag = TAG_DONE;               t[n].ti_Data = 0;
    sr = (struct ScreenModeRequester *)AllocAslRequest(ASL_ScreenModeRequest, t);
    if (sr && AslRequest(sr, 0)) {
        *out_id = (unsigned long)sr->sm_DisplayID;
        *out_depth = (int)sr->sm_DisplayDepth;
        ok = 1;
    }
    if (sr)
        FreeAslRequest(sr);
    CloseLibrary(AslBase);
    return ok;
}
