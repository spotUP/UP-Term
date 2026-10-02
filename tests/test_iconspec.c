#include <string.h>
#include "harness.h"
#include "../install/iconspec.h"

/* The Installer's "Shell icon opens UP-Term": only the device changes. */
static void shell_window_device(void)
{
    char out[128];
    CHECK_INT(iconspec_window_device("KCON:0/50//130/AmigaShell/CLOSE", "XCON", out, sizeof(out)), 1);
    CHECK_STR(out, "XCON:0/50//130/AmigaShell/CLOSE");
    CHECK_INT(iconspec_window_device("CON:0/50//130/AmigaShell/CLOSE", "XCON", out, sizeof(out)), 1);
    CHECK_STR(out, "XCON:0/50//130/AmigaShell/CLOSE");
    CHECK_INT(iconspec_window_device("XCON:0/50//130/AmigaShell/CLOSE", "XCON", out, sizeof(out)), 0);
    CHECK_INT(iconspec_window_device(0, "XCON", out, sizeof(out)), 1);  /* no tooltype: the default */
    CHECK_STR(out, "XCON:0/50//130/AmigaShell/CLOSE");
    CHECK_INT(iconspec_window_device("CON:", "XCON", out, sizeof(out)), 1);
    CHECK_STR(out, "XCON:");
    CHECK_INT(iconspec_window_device("0/0/640/200/Shell", "XCON", out, sizeof(out)), 1);
    CHECK_STR(out, "XCON:0/0/640/200/Shell");
    CHECK_INT(iconspec_window_device("CON:0/50//130/AmigaShell/CLOSE", "XCON", out, 10), -1);
}

void suite_iconspec(void)
{
    shell_window_device();
}
