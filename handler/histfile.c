/* histfile -- see histfile.h. */
#include <string.h>
#include "histfile.h"
#include "../claude/datadir.h"
#include "../claude/path.h"
#include "../claude/histline.h"

int hf_path(cl_sys *sys, const char *shell_file, char *out, long cap)
{
    char dir[200];
    if (shell_file && *shell_file && *shell_file != '\n')
        return hl_spec_path(shell_file, out, cap) < 0 ? -1 : 0;
    if (dd_dir(sys, HF_PROG, dir, sizeof(dir)) == 1) {
        if (path_join(dir, HF_NAME, out, cap))
            return -1;
        dd_mkdirs(sys, dir);
        dd_migrate(sys, HF_OLD, out);
        return 0;
    }
    if ((long)strlen(HF_OLD) >= cap)
        return -1;
    strcpy(out, HF_OLD);
    return 0;
}

long hf_load(char *data, long n, const char *shell_file)
{
    const char *project = hl_spec_project(shell_file);
    return project ? hl_to_plain(data, n, project) : n;
}

long hf_line(const char *shell_file, const char *word, char *out, long cap)
{
    const char *project = hl_spec_project(shell_file);
    jw w;
    long n;
    if (!project) {
        n = (long)strlen(word);
        if (n + 2 > cap)
            return -1;
        memcpy(out, word, (size_t)n);
        out[n] = '\n';
        out[n + 1] = 0;
        return n + 1;
    }
    jw_fixed(&w, out, cap);
    hl_put(&w, word, project);
    return w.oom ? -1 : w.n;
}
