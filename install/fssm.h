/* install/fssm.h: is a device node's dol_Startup a FileSysStartupMsg, and which exec device does it name.
 *
 * Header-only and free of AmigaOS types, so the host tests drive the same code install/hardvol.c runs.
 * dol_Startup is whatever the mount gave the handler: a small integer, a BSTR (a network or AUX: handler's
 * startup string), or a BPTR to a FileSysStartupMsg. Before 2026-10-25 hv_disk took anything above 0x400 for
 * the message and read fssm_Device through it: a BSTR's characters became a pointer and the read went
 * anywhere. Memory is read through the caller, who says which addresses are real memory (exec's TypeOfMem
 * or the Kickstart ROM on the Amiga, the test's arena on the host); no address is read before it is checked.
 *
 * Layout (dos/filehandler.h): FileSysStartupMsg { ULONG fssm_Unit; BSTR fssm_Device; BPTR fssm_Environ;
 * ULONG fssm_Flags; }, DosEnvec { ULONG de_TableSize; ... }. A disk's DosEnvec has at least the geometry,
 * de_TableSize >= DE_UPPERCYL (10); no table is longer than FSSM_ENV_MAX longs. */
#ifndef FSSM_H
#define FSSM_H

#define FSSM_NONE 0     /* not a startup message, or one that names no device */
#define FSSM_DEVICE 1   /* names an exec device */
#define FSSM_FLOPPY 2   /* names trackdisk.device */

#define FSSM_DEVICE_OFF 4
#define FSSM_ENVIRON_OFF 8
#define FSSM_SIZE 16
#define FSSM_ENV_MIN 10
#define FSSM_ENV_MAX 64

struct fssm_mem {
    int (*ok)(unsigned long addr, unsigned long len); /* addr..addr+len-1 is real memory */
    unsigned long (*rl)(unsigned long addr);          /* a big-endian long at addr */
    unsigned char (*rb)(unsigned long addr);          /* a byte at addr */
};

static int fssm_is(const struct fssm_mem *m, unsigned long addr, const char *s, int n)
{
    for (; n > 0; n--, addr++, s++) {
        unsigned char x = m->rb(addr);
        if (x >= 'A' && x <= 'Z')
            x += 32;
        if (x != (unsigned char)*s)
            return 0;
    }
    return 1;
}

/* startup: the device node's dol_Startup as it is. */
static int fssm_kind(const struct fssm_mem *m, unsigned long startup)
{
    unsigned long fs, env, dev, n;
    if (startup <= 0x400 || startup > 0x3FFFFFFFUL)
        return FSSM_NONE;
    fs = startup << 2;
    if (!m->ok(fs, FSSM_SIZE))
        return FSSM_NONE;
    env = m->rl(fs + FSSM_ENVIRON_OFF);
    dev = m->rl(fs + FSSM_DEVICE_OFF);
    if (!env || env > 0x3FFFFFFFUL || !dev || dev > 0x3FFFFFFFUL)
        return FSSM_NONE;
    env <<= 2;
    dev <<= 2;
    if (!m->ok(env, 4))
        return FSSM_NONE;
    n = m->rl(env);
    if (n < FSSM_ENV_MIN || n > FSSM_ENV_MAX || !m->ok(dev, 1))
        return FSSM_NONE;
    n = m->rb(dev);
    if (!n || !m->ok(dev, n + 1))
        return FSSM_NONE;
    if (n >= 9 && fssm_is(m, dev + 1, "trackdisk", 9))
        return FSSM_FLOPPY;
    return FSSM_DEVICE;
}
#endif
