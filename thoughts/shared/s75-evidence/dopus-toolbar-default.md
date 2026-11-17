# DOpus default lister toolbar: where it lives (s75) -- STOPPED, no code change
Verdict: the default is only in the binary environment file; no runtime (ARexx) way to change it. Per brief: stopped, no patcher written, no commit, no rig started.
Evidence (source /Users/spot/Code/Reference/dopus5/source/Program):
- environment.c:165 default `env->toolbar_path = "PROGDIR:Buttons/Toolbar"`; loaded from the env file by Library/config_open.c:1545-1548 (IFF chunk ID_TBAR); written by environment.c:976 (write_env_string ID_TBAR).
- lister.c:81 new lister copies `environment->toolbar_path`. GUI->toolbar (the cached bank new listers draw) is opened once in environment_open_toolbar (environment.c:336-365) and re-opened only on env change (main_reset.c:122-145).
- ARexx `lister set <h> toolbar` (function_set.c:182-190, rexx_lister.c:1269) and `lister new ... toolbar` (rexx_lister.c:70,217-220) are per lister only. rexx_data.c commands: no global toolbar/environment setter.
Options:
1. Binary patch of the ID_TBAR chunk of the shipped env at install (path string length changes the chunk size). Static: cannot follow theme switches or the per-screen set (font height/aspect/outline), so a theme switch leaves new listers on the old set.
2. Overwrite the file DOpus5:Buttons/Toolbar with a copy of the current set's bank at install and at each theme apply. No env edit, follows the set; but destroys the user's own Toolbar (needs backup) and new listers still use the cached GUI->toolbar until DOpus re-reads it (needs `dopus reload`/env reload; unverified on a rig).
3. Open-lister switch on a trigger: dopus_toolbar_flat already handles open listers; add a cheap re-run (e.g. on every Amarchy menu/status tick or when a new lister handle appears in `lister query all`). Robust to theme switches, small latency, no DOpus file changes.
4. Recommended: 3 with a lister-count check, else 2 after a rig test of reload behaviour. Shot recipe fix (re-apply after `lister new`) is the zero-risk stopgap.
