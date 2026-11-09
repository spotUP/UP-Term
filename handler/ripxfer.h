/* ripxfer.h -- RIPscrip level-9 block transfer, the receiving half (RIP-7b). Pure C89, no
 * OS calls: the line is a zm_line, the saved file goes to a vt_dl_sink, so the same code
 * runs on the host tests and in the handler. */
#ifndef RIPXFER_H
#define RIPXFER_H

#include "../engine/vtengine.h"
#include "../zm/zmodem.h"

/* A file larger than this is refused (the sender is told to skip it), as iTerm2 downloads. */
#define RIPXFER_MAX (4L * 1024L * 1024L)

/* name as ripxfer_name83 writes it: at most 12 characters and a NUL */
#define RIPXFER_NAME 13

/* raw (rn bytes) as an 8.3 file name: base name only (the shared dl_clean_name), characters
 * outside A-Z a-z 0-9 _ - ! # $ % & ' ( ) @ ^ ~ turned into '_', the stem cut to 8, the
 * extension (after the LAST dot) to 3. Returns 1; 0 when nothing usable is left (out ""). */
int ripxfer_name83(const char *raw, int rn, char *out);

/* Receive with ZMODEM (zm_receive) on `line`, after the engine's rip_block callback fired.
 * rip_name is the name the BBS gave in the RIP command ("" = use the sender's ZMODEM name);
 * host is the sanitised host directory ("" = "local"). Each complete file goes to sink as
 * (host, 8.3 name, bytes). *delivered counts the files the sink took. Returns zm_receive's
 * ZM_* result. */
int ripxfer_receive(const zm_line *line, vt_dl_sink sink, const char *host, const char *rip_name,
                    int *delivered);

#endif
