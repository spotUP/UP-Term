/* The session daemon of a key login (plan nas-claude-local, NL14), the parts of it that are
 * plain text and bytes, so the host tests can pin them: what the asynchronous shell runs and
 * which wasabid to run (the AGENT message is tn_announce_agent, net/tn.c). uptelnet.c does the Amiga half (the shell, the signal). */
#ifndef UPAGENT_H
#define UPAGENT_H

#define UPAGENT_DEFAULT "UP-Term:bin/wasabid" /* tried first, then C:wasabid */
#define UPAGENT_FALLBACK "C:wasabid"
#define UPAGENT_PORT 1240                     /* wasabid's own port is 1234 */
#define UPAGENT_NAME "uptelnet-agent"         /* the shell's process name: how it is found to be stopped */

/* The two lines of the script the shell executes: the key as a LOCAL shell variable (wasabid
 * reads GetVar("wasabi.key", ..., 0): a local variable wins over ENV:, so the boot wasabid and
 * its ENV: key are untouched), then the daemon on its own port. "keyhex" is the 64 digits of
 * tn_session_key. Text length, 0 when it does not fit in n bytes. */
int upagent_script(char *out, int n, const char *agent, long port, const char *keyhex);

/* The wasabid to run: UPAGENT_DEFAULT when exists() says it is there, else UPAGENT_FALLBACK when
 * that is; 0 when neither (uptelnet then starts no daemon and says 0 to the NAS). */
const char *upagent_pick(int (*exists)(const char *path));

#endif
