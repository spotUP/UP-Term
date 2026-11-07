/* The pure half of the Amarchy demo tour: see demo_core.h. */
#include "demo_core.h"

static const struct { const char *name; int secs; } g_scene[DEMO_SCENES] = {
    { "Windows move, size, zip", 12 },
    { "Snap", 12 },
    { "Focus and cycle", 10 },
    { "Tiling layouts", 14 },
    { "Themes", 14 },
    { "Desktop look", 10 },
    { "Status bar", 12 },
    { "Notices and clips", 10 },
    { "Capture", 10 },
    { "Launcher and packages", 12 },
};

const char *demo_scene_name(int i) { return (i >= 0 && i < DEMO_SCENES) ? g_scene[i].name : ""; }
int demo_scene_secs(int i) { return (i >= 0 && i < DEMO_SCENES) ? g_scene[i].secs : 0; }

int demo_total_secs(void)
{
    int i, s = 0;
    for (i = 0; i < DEMO_SCENES; i++)
        s += g_scene[i].secs;
    return s;
}

void demo_clock_init(demo_clock *c)
{
    c->running = 0;
    c->scene = 0;
    c->t = 0;
    c->loops = 0;
    c->fresh = 0;
}

static char up(char ch) { return (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : ch; }

static int word(const char *s, const char *w)
{
    while (*w)
        if (up(*s++) != *w++)
            return 0;
    return *s == 0 || *s == ' ' || *s == '\t';
}

demo_verb demo_parse(const char *args, int *scene)
{
    int n = 0;
    *scene = 1;
    while (*args == ' ' || *args == '\t')
        args++;
    if (!*args)
        return DV_START;
    if (word(args, "STOP"))
        return DV_STOP;
    if (word(args, "STATUS"))
        return DV_STATUS;
    if (*args < '0' || *args > '9')
        return DV_BAD;
    while (*args >= '0' && *args <= '9' && n < 1000)
        n = n * 10 + (*args++ - '0');
    while (*args == ' ' || *args == '\t')
        args++;
    if (*args || n < 1 || n > DEMO_SCENES)
        return DV_BAD;
    *scene = n;
    return DV_START;
}

void demo_start(demo_clock *c, int scene)
{
    demo_clock_init(c);
    c->running = 1;
    c->scene = (scene >= 1 && scene <= DEMO_SCENES) ? scene - 1 : 0;
    c->fresh = 1;
}

void demo_stop(demo_clock *c)
{
    c->running = 0;
    c->fresh = 0;
}

int demo_step(demo_clock *c, int *t, int *first)
{
    if (!c->running)
        return -1;
    if (c->t >= g_scene[c->scene].secs) {
        c->scene++;
        if (c->scene >= DEMO_SCENES) {
            c->scene = 0;
            c->loops++;
        }
        c->t = 0;
        c->fresh = 1;
    }
    *t = c->t;
    *first = c->fresh;
    c->t++;
    c->fresh = 0;
    return c->scene;
}

void demo_next(demo_clock *c)
{
    if (c->running)
        c->t = g_scene[c->scene].secs;
}

static char *put(char *p, char *end, const char *s)
{
    while (*s && p < end)
        *p++ = *s++;
    return p;
}

static char *putnum(char *p, char *end, int n)
{
    char tmp[12];
    int i = 0;
    if (n <= 0)
        tmp[i++] = '0';
    while (n > 0 && i < 11) {
        tmp[i++] = (char)('0' + n % 10);
        n /= 10;
    }
    while (i > 0 && p < end)
        *p++ = tmp[--i];
    return p;
}

void demo_status_text(const demo_clock *c, char *out, int cap)
{
    char *p = out, *end = out + cap - 1;
    if (cap < 1)
        return;
    p = put(p, end, "demo running=");
    p = putnum(p, end, c->running ? 1 : 0);
    p = put(p, end, " scene=");
    p = putnum(p, end, c->running ? c->scene + 1 : 0);
    p = put(p, end, " loops=");
    p = putnum(p, end, c->loops);
    *p = 0;
}
