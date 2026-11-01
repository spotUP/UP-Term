/* install/fssm.h: a device node's dol_Startup is read as a FileSysStartupMsg only when it is one. */
#include "harness.h"
#include "../install/fssm.h"

/* 4 KB of "real memory" at 0x10000; every read outside it is counted */
#define BASE 0x10000UL
#define SIZE 4096UL

struct arena {
    unsigned char b[SIZE];
    int stray;
};

static struct arena a;

static int in(unsigned long addr, unsigned long len)
{
    return addr >= BASE && len <= SIZE && addr - BASE <= SIZE - len;
}

static int a_ok(unsigned long addr, unsigned long len)
{
    return len && in(addr, len);
}

static unsigned long a_rl(unsigned long addr)
{
    const unsigned char *p;
    if (!in(addr, 4)) {
        a.stray++;
        return 0;
    }
    p = a.b + (addr - BASE);
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static unsigned char a_rb(unsigned long addr)
{
    if (!in(addr, 1)) {
        a.stray++;
        return 0;
    }
    return a.b[addr - BASE];
}

static void put_l(unsigned long addr, unsigned long v)
{
    unsigned char *p = a.b + (addr - BASE);
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void put_bstr(unsigned long addr, const char *s)
{
    a.b[addr - BASE] = (unsigned char)strlen(s);
    memcpy(a.b + (addr - BASE) + 1, s, strlen(s));
}

/* a startup message at 0x10100 naming dev, its DosEnvec at 0x10200 with tablesize longs */
static unsigned long msg(const char *dev, unsigned long tablesize)
{
    memset(&a, 0, sizeof a);
    put_bstr(0x10300, dev);
    put_l(0x10100, 0);                  /* fssm_Unit */
    put_l(0x10100 + 4, 0x10300 >> 2);   /* fssm_Device */
    put_l(0x10100 + 8, 0x10200 >> 2);   /* fssm_Environ */
    put_l(0x10200, tablesize);
    return 0x10100 >> 2;
}

static void kinds(void)
{
    struct fssm_mem m;
    unsigned long st;
    m.ok = a_ok;
    m.rl = a_rl;
    m.rb = a_rb;

    st = msg("scsi.device", 16);       /* a hard disk partition mounted from the RDB */
    CHECK_INT(fssm_kind(&m, st), FSSM_DEVICE);
    st = msg("TrackDisk.device", 16);  /* a floppy, whatever the unit is called */
    CHECK_INT(fssm_kind(&m, st), FSSM_FLOPPY);
    st = msg("", 16);                  /* no device name */
    CHECK_INT(fssm_kind(&m, st), FSSM_NONE);
    st = msg("scsi.device", 3);        /* an environment too short for a disk */
    CHECK_INT(fssm_kind(&m, st), FSSM_NONE);
    CHECK_INT(fssm_kind(&m, 2), FSSM_NONE); /* a small integer startup (AUX:, PIPE:) */
    CHECK_INT(a.stray, 0);

    /* a BSTR startup (a network handler's "host:share" string): its characters are no pointers, and nothing
     * outside real memory is read */
    memset(&a, 0, sizeof a);
    put_bstr(0x10400, "server.lan:/export/amiga/work");
    CHECK_INT(fssm_kind(&m, 0x10400 >> 2), FSSM_NONE);
    CHECK_INT(a.stray, 0);

    /* a startup BPTR to memory that is not there */
    CHECK_INT(fssm_kind(&m, 0x00800000UL >> 2), FSSM_NONE);
    CHECK_INT(a.stray, 0);

    /* a message whose device name runs off the end of memory */
    msg("scsi.device", 16);
    put_l(0x10100 + 4, (BASE + SIZE - 4) >> 2);
    a.b[SIZE - 4] = 40;
    CHECK_INT(fssm_kind(&m, 0x10100 >> 2), FSSM_NONE);
    CHECK_INT(a.stray, 0);
}

void suite_fssm(void)
{
    kinds();
}
