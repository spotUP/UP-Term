/* histfile -- see histfile.h. */
#include <string.h>
#include "histfile.h"
#include "../claude/datadir.h"
#include "../claude/path.h"

int hf_path(cl_sys *sys, const char *shell_file, char *out, long cap)
{
    char dir[200];
    if (shell_file && *shell_file) {
        if ((long)strlen(shell_file) >= cap)
            return -1;
        strcpy(out, shell_file);
        return 0;
    }
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
