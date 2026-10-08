/* See complete_core.h. Portable C89: no OS calls. */
#include <string.h>
#include "complete_core.h"
#include "vtcon_packets.h"

int cc_is_command(long entry_type, unsigned long protection)
{
    if (entry_type >= 0)
        return 0; /* a directory (or no type): never a command */
    return !(protection & CC_FIBF_EXECUTE) || (protection & CC_FIBF_SCRIPT) != 0;
}

int cc_resident_listed(long seg_uc)
{
    return seg_uc >= 0 || seg_uc == CC_CMD_INTERNAL;
}

typedef struct cc_seen {
    long lock[CC_DIRS_MAX];
    int own[CC_DIRS_MAX];    /* a c_next lock: dropped at the end */
    int n;
} cc_seen;

/* one directory: skipped when already seen, else visited and remembered */
static int cc_step(const cc_dirs_os *os, void *u, cc_seen *s, long d, int own)
{
    int i, r;
    for (i = 0; i < s->n; i++)
        if (os->same(s->lock[i], d)) {
            if (own)
                os->drop(d);
            return 0;
        }
    r = os->visit(u, d);
    if (s->n < CC_DIRS_MAX) {
        s->lock[s->n] = d;
        s->own[s->n] = own;
        s->n++;
    } else if (own) {
        os->drop(d);
    }
    return r;
}

int cc_walk_command_dirs(const cc_dirs_os *os, void *u)
{
    cc_seen s;
    long d;
    int r = 0, i;
    s.n = 0;
    while (!r && (d = os->c_next(u)) != 0)
        r = cc_step(os, u, &s, d, 1);
    os->c_end(u);
    while (!r && (d = os->p_next(u)) != 0)
        r = cc_step(os, u, &s, d, 0);
    for (i = 0; i < s.n; i++)
        if (s.own[i])
            os->drop(s.lock[i]);
    return r;
}

/* ---- W22: the command-name cache (see complete_core.h) -------------------- */

static int cc_lower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
        return c + 32; /* Latin-1 capitals: AmigaDOS names ignore case */
    return c;
}

static int cc_same_name(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (cc_lower((unsigned char)*a) != cc_lower((unsigned char)*b))
            return 0;
    return *a == *b;
}

static int cc_date_eq(const cc_date *a, const cc_date *b)
{
    return a->days == b->days && a->minute == b->minute && a->tick == b->tick;
}

static void cc_lock(cc_cache *c, const cc_cache_os *os)
{
    os->lock();
    c->in_use = 1;
}

static void cc_unlock(cc_cache *c, const cc_cache_os *os)
{
    c->in_use = 0;
    os->unlock();
}

void cc_cache_init(cc_cache *c, long cap, long keep)
{
    memset(c, 0, sizeof(*c));
    c->cap = cap;
    c->keep = keep;
}

static int cc_find(const cc_cache *c, const char *name)
{
    int i;
    for (i = 0; i < CC_CACHE_DIRS; i++)
        if (c->e[i].buf && cc_same_name(c->e[i].buf, name))
            return i;
    return -1;
}

static void cc_drop(cc_cache *c, const cc_cache_os *os, int i)
{
    os->free(c->e[i].buf, c->e[i].size);
    c->bytes -= c->e[i].size;
    c->e[i].buf = 0;
    c->e[i].size = 0;
    c->loaded = 0; /* the file may have it: offer the file again */
}

/* the least recently used entry, or -1 when there is none */
static int cc_lru(const cc_cache *c)
{
    int i, best = -1;
    for (i = 0; i < CC_CACHE_DIRS; i++)
        if (c->e[i].buf && (best < 0 || c->e[i].used < c->e[best].used))
            best = i;
    return best;
}

/* An entry (buf: name NUL names..., size bytes) into RAM in place of the
 * directory's old one; the least recently used go while it would pass the
 * cap. used 0 (an entry of the file, loaded unasked) never pushes out one
 * that was used. Returns its slot, or -1 when it is not kept. */
static int cc_install(cc_cache *c, const cc_cache_os *os, const char *buf, long size,
                      const cc_date *d, unsigned long used)
{
    int i = cc_find(c, buf), k;
    char *copy;
    if (size > c->cap)
        return -1;
    if (i >= 0)
        cc_drop(c, os, i);
    for (;;) {
        for (i = 0; i < CC_CACHE_DIRS && c->e[i].buf; i++)
            ;
        if (c->bytes + size <= c->cap && i < CC_CACHE_DIRS)
            break;
        k = cc_lru(c);
        if (k < 0 || (!used && c->e[k].used))
            return -1;
        cc_drop(c, os, k);
    }
    copy = (char *)os->alloc(size);
    if (!copy)
        return -1;
    memcpy(copy, buf, (size_t)size);
    c->e[i].buf = copy;
    c->e[i].size = size;
    c->e[i].nlen = (long)strlen(copy) + 1;
    c->e[i].date = *d;
    c->e[i].used = used;
    c->bytes += size;
    return i;
}

static void cc_emit(const char *buf, long size, cc_name_fn emit, void *x)
{
    long k;
    if (!emit)
        return;
    for (k = (long)strlen(buf) + 1; k < size; k += (long)strlen(buf + k) + 1)
        if (!emit(x, buf + k))
            return;
}

/* -- the file: "UPCC", version, count, payload bytes (32 bits each, high
 * byte first), the entries (days, minute, tick, size, then the buffer),
 * and a checksum of everything before it -- */

static void cc_put32(char *p, long v)
{
    unsigned long w = (unsigned long)v;
    p[0] = (char)((w >> 24) & 0xFF);
    p[1] = (char)((w >> 16) & 0xFF);
    p[2] = (char)((w >> 8) & 0xFF);
    p[3] = (char)(w & 0xFF);
}

static unsigned long cc_get32u(const char *p)
{
    const unsigned char *q = (const unsigned char *)p;
    return ((unsigned long)q[0] << 24) | ((unsigned long)q[1] << 16) | ((unsigned long)q[2] << 8) | q[3];
}

static long cc_get32(const char *p)
{
    unsigned long v = cc_get32u(p);
    return (v & 0x80000000UL) ? -(long)(~v & 0x7FFFFFFFUL) - 1 : (long)v;
}

static long cc_sum(const char *p, long n)
{
    unsigned long s = 0;
    long i;
    for (i = 0; i < n; i++)
        s = (s * 31 + (unsigned char)p[i]) & 0xFFFFFFFFUL;
    return (s & 0x80000000UL) ? -(long)(~s & 0x7FFFFFFFUL) - 1 : (long)s;
}

/* is buf (size bytes) an entry: a name, then names, a NUL after each? */
static int cc_entry_ok(const char *buf, long size)
{
    long k;
    if (size < 2 || buf[size - 1] || !buf[0])
        return 0;
    for (k = 0; buf[k]; k++)
        ;
    return k < CC_CACHE_NAMELEN;
}

int cc_cache_parse(const char *buf, long len,
                   int (*each)(void *x, const cc_date *d, const char *ent, long size), void *x)
{
    long count, payload, end, at, i, size;
    if (!buf || len < CC_CACHE_HEAD + 4 || memcmp(buf, "UPCC", 4) ||
        cc_get32(buf + 4) != CC_CACHE_VERSION)
        return -1;
    count = cc_get32(buf + 8);
    payload = cc_get32(buf + 12);
    if (count < 0 || count > CC_CACHE_DIRS || payload < 0 || payload > CC_CACHE_FILE_MAX ||
        CC_CACHE_HEAD + payload + 4 != len)
        return -1;
    end = CC_CACHE_HEAD + payload;
    if (cc_get32(buf + end) != cc_sum(buf, end))
        return -1;
    /* every entry checked before any is given */
    for (at = CC_CACHE_HEAD, i = 0; i < count; i++) {
        if (at + CC_CACHE_EHEAD > end)
            return -1;
        size = cc_get32(buf + at + 12);
        if (size < 0 || size > end - at - CC_CACHE_EHEAD || !cc_entry_ok(buf + at + CC_CACHE_EHEAD, size))
            return -1;
        at += CC_CACHE_EHEAD + size;
    }
    if (at != end)
        return -1;
    for (at = CC_CACHE_HEAD, i = 0; i < count; i++) {
        cc_date d;
        d.days = cc_get32(buf + at);
        d.minute = cc_get32(buf + at + 4);
        d.tick = cc_get32(buf + at + 8);
        size = cc_get32(buf + at + 12);
        if (each && !each(x, &d, buf + at + CC_CACHE_EHEAD, size))
            break;
        at += CC_CACHE_EHEAD + size;
    }
    return (int)count;
}

typedef struct cc_packer {
    const cc_cache *c;
    char *out;
    long max, at, names;  /* names: the buffers' bytes so far */
    int count;
} cc_packer;

static int cc_pack_one(void *x, const cc_date *d, const char *ent, long size)
{
    cc_packer *p = (cc_packer *)x;
    if (p->count >= CC_CACHE_DIRS || p->names + size > CC_CACHE_CAP ||
        p->at + CC_CACHE_EHEAD + size + 4 > p->max)
        return 1; /* no room: a smaller one may still fit */
    cc_put32(p->out + p->at, d->days);
    cc_put32(p->out + p->at + 4, d->minute);
    cc_put32(p->out + p->at + 8, d->tick);
    cc_put32(p->out + p->at + 12, size);
    memcpy(p->out + p->at + CC_CACHE_EHEAD, ent, (size_t)size);
    p->at += CC_CACHE_EHEAD + size;
    p->names += size;
    p->count++;
    return 1;
}

/* an old file's entry, for a directory RAM lacks */
static int cc_pack_old(void *x, const cc_date *d, const char *ent, long size)
{
    cc_packer *p = (cc_packer *)x;
    if (cc_find(p->c, ent) >= 0)
        return 1;
    return cc_pack_one(x, d, ent, size);
}

long cc_cache_pack(const cc_cache *c, const char *old, long oldlen, char *out, long max)
{
    cc_packer p;
    int order[CC_CACHE_DIRS], n = 0, i, j;
    if (max < CC_CACHE_HEAD + 4)
        return 0;
    p.c = c;
    p.out = out;
    p.max = max;
    p.at = CC_CACHE_HEAD;
    p.names = 0;
    p.count = 0;
    /* RAM's, the most recently used first: a full file keeps those */
    for (i = 0; i < CC_CACHE_DIRS; i++) {
        if (!c->e[i].buf)
            continue;
        for (j = n; j > 0 && c->e[order[j - 1]].used < c->e[i].used; j--)
            order[j] = order[j - 1];
        order[j] = i;
        n++;
    }
    for (i = 0; i < n; i++)
        cc_pack_one(&p, &c->e[order[i]].date, c->e[order[i]].buf, c->e[order[i]].size);
    if (old)
        cc_cache_parse(old, oldlen, cc_pack_old, &p);
    memcpy(out, "UPCC", 4);
    cc_put32(out + 4, CC_CACHE_VERSION);
    cc_put32(out + 8, p.count);
    cc_put32(out + 12, p.at - CC_CACHE_HEAD);
    cc_put32(out + p.at, cc_sum(out, p.at));
    return p.at + 4;
}

/* -- loading, looking up, warming -- */

typedef struct cc_loader {
    cc_cache *c;
    const cc_cache_os *os;
} cc_loader;

static int cc_load_one(void *x, const cc_date *d, const char *ent, long size)
{
    cc_loader *l = (cc_loader *)x;
    if (cc_find(l->c, ent) < 0)
        cc_install(l->c, l->os, ent, size, d, 0);
    return 1;
}

/* the file's entries RAM lacks, into RAM (the lock held) */
static void cc_load_file(cc_cache *c, const cc_cache_os *os)
{
    char *buf;
    long n;
    cc_loader l;
    c->loaded = 1;
    if (c->file_stale)
        return;
    buf = (char *)os->alloc(CC_CACHE_FILE_MAX);
    if (!buf)
        return;
    n = os->load(buf, CC_CACHE_FILE_MAX);
    l.c = c;
    l.os = os;
    if (n > 0)
        cc_cache_parse(buf, n, cc_load_one, &l);
    os->free(buf, CC_CACHE_FILE_MAX);
    c->loaded = 1; /* cc_install's evictions cleared it: the file was offered */
}

/* a scan's names, packed after the directory's name */
typedef struct cc_build {
    const cc_cache_os *os;
    char *buf;
    long len, cap;
    int failed;
} cc_build;

static int cc_build_put(cc_build *b, const char *s)
{
    long n = (long)strlen(s) + 1;
    if (b->len + n > b->cap) {
        long cap = b->cap ? b->cap * 2 : 1024;
        char *more;
        while (cap < b->len + n)
            cap *= 2;
        more = (char *)b->os->alloc(cap);
        if (!more) {
            b->failed = 1;
            return 0;
        }
        if (b->buf) {
            memcpy(more, b->buf, (size_t)b->len);
            b->os->free(b->buf, b->cap);
        }
        b->buf = more;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, (size_t)n);
    b->len += n;
    return 1;
}

static int cc_build_add(void *x, const char *name)
{
    return cc_build_put((cc_build *)x, name);
}

int cc_cache_dir(cc_cache *c, const cc_cache_os *os, void *u, long lock, int cold,
                 cc_name_fn emit, void *x)
{
    char name[CC_CACHE_NAMELEN];
    cc_date d;
    cc_build b;
    int i, ok;
    if (!os->stat(u, lock, name, (int)sizeof(name), &d) || !name[0])
        return CC_NOSTAT;
    cc_lock(c, os);
    i = cc_find(c, name);
    if (i < 0 && !c->loaded) {
        cc_load_file(c, os);
        i = cc_find(c, name);
    }
    if (i >= 0 && (cc_date_eq(&c->e[i].date, &d) || !cold)) {
        int fresh = cc_date_eq(&c->e[i].date, &d);
        c->e[i].used = ++c->clock;
        cc_emit(c->e[i].buf, c->e[i].size, emit, x);
        cc_unlock(c, os);
        return fresh ? CC_FRESH : CC_STALE;
    }
    cc_unlock(c, os);
    if (!cold)
        return CC_MISSING;
    /* read it: without the lock (a Tab meanwhile answers from the rest),
     * into a buffer of its own */
    b.os = os;
    b.buf = 0;
    b.len = b.cap = 0;
    b.failed = 0;
    ok = cc_build_put(&b, name) && os->scan(u, lock, cc_build_add, &b) && !b.failed;
    cc_lock(c, os);
    c->scans++;
    if (ok) {
        cc_emit(b.buf, b.len, emit, x);
        if (cc_install(c, os, b.buf, b.len, &d, ++c->clock) >= 0)
            c->dirty = 1;
    }
    cc_unlock(c, os);
    if (b.buf)
        os->free(b.buf, b.cap);
    return ok ? CC_FRESH : CC_MISSING;
}

typedef struct cc_cn {
    cc_cache *c;
    const cc_cache_os *os;
    const cc_dirs_os *dirs;
    void *u;
    int cold, partial;
    cc_name_fn emit;
    void *x;
} cc_cn;

static long cc_cn_c_next(void *x) { cc_cn *k = (cc_cn *)x; return k->dirs->c_next(k->u); }
static void cc_cn_c_end(void *x) { cc_cn *k = (cc_cn *)x; k->dirs->c_end(k->u); }
static long cc_cn_p_next(void *x) { cc_cn *k = (cc_cn *)x; return k->dirs->p_next(k->u); }

static int cc_cn_visit(void *x, long lock)
{
    cc_cn *k = (cc_cn *)x;
    int r = cc_cache_dir(k->c, k->os, k->u, lock, k->cold, k->emit, k->x);
    if (r == CC_NOSTAT && k->emit)
        r = k->os->scan(k->u, lock, k->emit, k->x) ? CC_FRESH : CC_MISSING; /* uncached */
    if (r == CC_STALE || r == CC_MISSING)
        k->partial = 1;
    return 0;
}

int cc_command_names(cc_cache *c, const cc_cache_os *os, const cc_dirs_os *dirs, void *u,
                     int cold, cc_name_fn emit, void *x)
{
    cc_dirs_os w;
    cc_cn k;
    w.c_next = cc_cn_c_next;
    w.c_end = cc_cn_c_end;
    w.p_next = cc_cn_p_next;
    w.same = dirs->same;
    w.drop = dirs->drop;
    w.visit = cc_cn_visit;
    k.c = c;
    k.os = os;
    k.dirs = dirs;
    k.u = u;
    k.cold = cold;
    k.partial = 0;
    k.emit = emit;
    k.x = x;
    cc_walk_command_dirs(&w, &k);
    return k.partial;
}

void cc_cache_warm(cc_cache *c, const cc_cache_os *os, const cc_dirs_os *dirs, void *u)
{
    cc_lock(c, os);
    c->warmups++;
    cc_unlock(c, os);
    cc_command_names(c, os, dirs, u, 1, 0, 0);
    cc_cache_end(c, os);
}

void cc_cache_end(cc_cache *c, const cc_cache_os *os)
{
    int k;
    cc_lock(c, os);
    if (c->dirty) {
        char *out = (char *)os->alloc(CC_CACHE_FILE_MAX);
        /* the old file merged in only when RAM may lack some of it */
        char *old = c->file_stale || c->loaded ? 0 : (char *)os->alloc(CC_CACHE_FILE_MAX);
        long oldlen = old ? os->load(old, CC_CACHE_FILE_MAX) : -1;
        if (out) {
            long n = cc_cache_pack(c, oldlen > 0 ? old : 0, oldlen, out, CC_CACHE_FILE_MAX);
            if (n && os->save(out, n)) {
                c->dirty = 0;
                c->file_stale = 0;
            }
            os->free(out, CC_CACHE_FILE_MAX);
        }
        if (old)
            os->free(old, CC_CACHE_FILE_MAX);
    }
    /* RAM down to keep; what the file has not got stays */
    while (!c->dirty && c->bytes > c->keep && (k = cc_lru(c)) >= 0)
        cc_drop(c, os, k);
    cc_unlock(c, os);
}

void cc_cache_reset(cc_cache *c, const cc_cache_os *os)
{
    int i;
    cc_lock(c, os);
    for (i = 0; i < CC_CACHE_DIRS; i++)
        c->e[i].date.days = -1; /* no directory has that date: read again */
    c->file_stale = 1;          /* nor is the file's date believed */
    c->loaded = 1;
    cc_unlock(c, os);
}

long cc_cache_release(cc_cache *c, const cc_cache_os *os)
{
    long freed = c->bytes;
    int i;
    if (c->in_use)
        return 0;
    for (i = 0; i < CC_CACHE_DIRS; i++)
        if (c->e[i].buf)
            cc_drop(c, os, i);
    return freed;
}

/* ---- W21: ghost text from completion candidates ------------------------- */

int cc_ghost_split(const char *word, char *dir, int max)
{
    int i, split = 0;
    for (i = 0; word[i]; i++)
        if (word[i] == '/' || word[i] == ':')
            split = i + 1;
    if (split > max - 1)
        split = max - 1;
    memcpy(dir, word, split);
    dir[split] = 0;
    return split;
}

static int ghost_lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int cc_ghost_tail(const char *names, long len, const char *prefix, char *tail, int max)
{
    long k = 0;
    int pn = (int)strlen(prefix);
    while (k < len) {
        const char *e = names + k;
        int n = (int)strlen(e), i;
        k += n + 1;
        if (n <= pn)
            continue; /* the prefix is all of it (its suffix is no tail) */
        for (i = 0; i < pn && ghost_lower((unsigned char)e[i]) == ghost_lower((unsigned char)prefix[i]); i++)
            ;
        if (i < pn)
            continue;
        if (e[n - 1] == ' ')
            n--;
        n -= pn;
        if (n <= 0)
            continue;
        if (n > max - 1)
            n = max - 1;
        memcpy(tail, e + pn, n);
        tail[n] = 0;
        return n;
    }
    tail[0] = 0;
    return 0;
}

int cc_ghost_from_add(const char *add, char *tail, int max)
{
    int n = (int)strlen(add);
    if (n && add[n - 1] == ' ')
        n--;
    if (n > max - 1)
        n = max - 1;
    memcpy(tail, add, n);
    tail[n] = 0;
    return n;
}

/* V93: the marker line (vtcon_packets.h ACTION_VTCON_COMPLETE) */
int cc_mark_line(const char *line, int point, char *out, int max)
{
    int m = (int)strlen(VTCON_COMPLETE_MARK), n = (int)strlen(line), k, d = 1, p = point;
    while (p >= 10) {
        p /= 10;
        d++;
    }
    if (point < 0 || point > n || m + d + 1 + n + 2 > max)
        return 0;
    memcpy(out, VTCON_COMPLETE_MARK, m);
    for (k = d - 1, p = point; k >= 0; k--, p /= 10)
        out[m + k] = (char)('0' + p % 10);
    out[m + d] = ' ';
    memcpy(out + m + d + 1, line, n);
    out[m + d + 1 + n] = '\n';
    out[m + d + 2 + n] = 0;
    return m + d + 2 + n;
}

int cc_common_start(const char *names, long len, char *out, int max)
{
    long k;
    int n = -1, i;
    for (k = 0; k < len; k += (long)strlen(names + k) + 1) {
        const char *w = names + k;
        if (n < 0) {
            for (n = 0; w[n] && n < max - 1; n++)
                out[n] = w[n];
            continue;
        }
        for (i = 0; i < n && out[i] == w[i]; i++)
            ;
        n = i;
    }
    if (n < 0)
        n = 0;
    out[n] = 0;
    return n;
}
