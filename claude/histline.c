/* histline -- see histline.h. */
#include <string.h>
#include "histline.h"

int hl_parse(const char *line, long n, jv *display, jv *project)
{
    jv v;
    if (n <= 0 || json_parse(line, n, &v) || json_type(v) != J_OBJ || !json_get(v, "display", display) ||
        json_type(*display) != J_STR)
        return 0;
    if (!json_get(v, "project", project) || json_type(*project) != J_STR) {
        project->p = "\"\"";
        project->n = 2;
    }
    return 1;
}

void hl_put(jw *w, const char *display, const char *project)
{
    jw_rawz(w, "{\"display\":");
    jw_strz(w, display);
    jw_rawz(w, ",\"project\":");
    jw_strz(w, project);
    jw_rawz(w, "}\n");
}

const char *hl_spec_project(const char *spec)
{
    const char *nl = spec ? strchr(spec, '\n') : 0;
    return nl ? nl + 1 : 0;
}

long hl_spec_path(const char *spec, char *out, long cap)
{
    const char *nl = strchr(spec, '\n');
    long n = nl ? (long)(nl - spec) : (long)strlen(spec);
    if (n >= cap)
        return -1;
    memcpy(out, spec, (size_t)n);
    out[n] = 0;
    return n;
}

long hl_to_plain(char *data, long n, const char *project)
{
    long a = 0, i, o = 0;
    for (i = 0; i < n; i++) {
        jv d, p;
        if (data[i] != '\n')
            continue;
        if (hl_parse(data + a, i - a, &d, &p) && json_streq(p, project)) {
            /* the decoded text is never longer than its line, and is written behind the read position */
            long l = json_str(d, data + o, i - a + 1);
            if (l >= 0) {
                long k;
                for (k = 0; k < l; k++)
                    if (data[o + k] == '\n' || data[o + k] == '\r')
                        data[o + k] = ' '; /* one line each in the console's editor */
                o += l;
                data[o++] = '\n';
            }
        }
        a = i + 1;
    }
    return o;
}
