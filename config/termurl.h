/* termurl: the text work behind OSC 7 (the shell's directory) and OSC 8
 * (hyperlinks) on the Amiga side, with no OS calls so the host suite tests
 * it (tests/test_protocol.c). vsh writes OSC 7 with termurl_osc7; the
 * window reads it back with termurl_cwd_dir (a new tab starts there) and
 * builds the command that opens a link with termurl_link_command.
 *
 * Paths: an AmigaDOS name "Work:src/a b" is the URL path "/Work/src/a%20b"
 * (the volume is the first component, as ixemul's Unix names have it).
 * Anything that goes into a command line or a script is refused when it
 * holds a quote, AmigaDOS's escape character '*', or a control character:
 * a program must not be able to type commands through a URL.
 *
 * Portable C89, as config/upconf.c. */
#ifndef TERMURL_H
#define TERMURL_H

/* OSC 7 for directory dir (an AmigaDOS name, "Vol:path") on host (may be
 * ""): ESC ] 7 ; file://host/Vol/path ESC \, the path percent-encoded.
 * The length written (NUL-terminated), 0 when it does not fit in max. */
long termurl_osc7(const char *dir, const char *host, char *out, long max);

/* The AmigaDOS directory an OSC 7 URL names, when it is on this machine:
 * the host part empty, "localhost", or myhost (no case). 1 and out filled
 * ("Vol:path"), 0 when it is remote, not a file: URL, does not fit, or holds
 * a character a script cannot quote. */
int termurl_cwd_dir(const char *uri, const char *myhost, char *out, int cap);

/* The command line that opens uri: tmpl with its %s replaced by the URI in
 * quotes (or the quoted URI after it when tmpl has none). 0 when uri holds
 * a quote, '*' or a control character, or it does not fit. */
int termurl_link_command(const char *tmpl, const char *uri, char *out, int cap);

#endif
