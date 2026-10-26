/* prefs_merge: Prefs holds one copy of the fields per session, and the Amarchy menu (or another Prefs) can
 * write the same files meanwhile. When the files change under an open Prefs, the pages the user has not
 * edited since open (or last save) take the file's values; the pages the user has edited keep the user's
 * edits, so a later Save loses neither (plan 2026-10-26-amarchy-prefs-inline.md, "Concurrency with Prefs").
 * The decision is per page, and a field's page is the page of its settings_def row: no field list here.
 * No OS calls; tests/test_prefs.c walks it. */
#ifndef PREFS_MERGE_H
#define PREFS_MERGE_H

#include "prefs_core.h"

/* The pages (bit 1 << PG_PAGE_*) with a setting where cur differs from orig: what the user edited. */
unsigned prefs_pages_edited(const prefs_fields *cur, const prefs_fields *orig);

/* cur: the fields now (gadgets collected), orig: what they held at open or last save, file: what the files
 * hold now. out gets cur on an edited page and file on every other; new_orig gets orig on an edited page
 * and file on every other (an edited page stays edited, an unedited one stays unedited). Fields no row
 * owns stay as in cur and orig. Returns the pages whose shown value changed (bit 1 << PG_PAGE_*), the ones
 * to refresh. out and new_orig must not alias the inputs. */
unsigned prefs_merge_external(const prefs_fields *cur, const prefs_fields *orig, const prefs_fields *file,
                              prefs_fields *out, prefs_fields *new_orig);

#endif
