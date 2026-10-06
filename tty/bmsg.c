/* Boundary messages (bmsg.h). */
#include "bmsg.h"
#include <string.h>

const char *bmsg_no_stack(void)
{
    return "No TCP/IP stack running (Roadshow or AmiTCP): start it, then try again";
}

const char *bmsg_net_errno(long err)
{
    switch (err) {
    case 65:
        return "no route to host (EHOSTUNREACH): check the address and your network";
    case 61:
        return "connection refused (ECONNREFUSED): nothing listens on that port";
    case 60:
        return "connection timed out (ETIMEDOUT): the host did not answer";
    default:
        return 0;
    }
}

const char *bmsg_amissl_missing(void)
{
    return "needs AmiSSL 5 for talking to Anthropic; with ENVARC:Claude/remote set it "
           "connects to the NAS instead";
}

const char *bmsg_uptelnet_missing(void)
{
    return "ENVARC:Claude/remote needs C:uptelnet, which is not there: run the UP-Term Install";
}

const char *bmsg_xcon_missing(void)
{
    return "UP-Term is not installed or XCON: is not mounted (reboot after Install)";
}

const char *bmsg_need_68020(void)
{
    return "needs a 68020 or better";
}

int bmsg_cpu_ok(unsigned attn)
{
    return (attn & 2u) != 0;
}

/* the commands of UP-Term:bin: GNU coreutils 5.2.1, sz, rz, hl, mdv and
 * the BebboSSH tools (ssh and scp are vsh functions that run them) */
static const char *const kit_bin[] = {
    "[", "basename", "cat", "chgrp", "chmod", "chown", "chroot", "cksum", "comm", "cp",
    "csplit", "cut", "date", "dd", "dir", "dircolors", "dirname", "du", "echo", "env", "expand",
    "expr", "factor", "false", "fmt", "fold", "groups", "head", "hostid", "hostname", "id",
    "install", "join", "kill", "link", "ln", "logname", "ls", "md5sum", "mkdir", "mkfifo",
    "mknod", "mv", "nice", "nl", "nohup", "od", "paste", "pathchk", "pinky", "pr", "printenv",
    "printf", "ptx", "pwd", "readlink", "rm", "rmdir", "seq", "sha1sum", "shred", "sleep",
    "sort", "split", "stat", "stty", "sum", "sync", "tac", "tail", "tee", "test", "touch", "tr",
    "true", "tsort", "tty", "uname", "unexpand", "uniq", "unlink", "users", "vdir", "wc", "who",
    "whoami", "yes", "sz", "rz", "hl", "mdv", "ssh", "scp", "bebbossh", "bebboscp",
    "bebbosshkeygen", "bebbosshd"
};

const char *bmsg_kit_drawer(const char *name)
{
    size_t i;
    if (!name || !*name)
        return 0;
    for (i = 0; i < sizeof(kit_bin) / sizeof(kit_bin[0]); i++)
        if (!strcmp(kit_bin[i], name))
            return BMSG_KIT_BIN;
    return 0;
}
