# DOpus splash vs lister vs button bank: window titles (rig 7, --os 32, DOpus 5.104, measured)

Method: `rig.py dopus --os 32` (install into rig 7 VTC:), `rig.py start --os 32`, then a poller (ami._conn + wasabi list_windows, every 3 s)
while `rig.py reboot` ran. Fields come from wasabid WINDOWS lines (`W addr left top w h bl bt br bb flags task title`).
LIMIT: wasabid reports only two flag letters, `a` = active, `b` = backdrop (`-` = neither). The full Intuition Flags hex was NOT read
(needs an Amiga-side probe or a Patina theme pick; not built, budget). Screen: 640x256x8.

## (a) splash up (one sample, 18:48:18; screen title "Workbench Screen", the only window)
| window | task | title | geometry | flags |
|--|--|--|--|--|
| splash | `dopus_progressbar` | `Directory Opus` | 150,103 339x49, borders 4/11/4/2 | `-` (not active, not backdrop) |

Screen title then: `Workbench Screen`. 3 s later (18:48:21) the splash is gone, the screen title is "Attempting to launch 'MenuTools'..."
and the listers/buttons exist. The text "Opus 5 loading... please wait." is drawn inside the window, not in its Title.
Only this one sample caught the splash (3 s poll, short-lived); treat the geometry as one observation.

## (b) after start (stable; screen title `Directory Opus  <chip> graphics  <fast> other`)
| window | task | title | geometry | flags |
|--|--|--|--|--|
| lister | `dopus_lister` | `RAM Disk,  1G free, 14K in use, 0% full` (path/free-space text, changes) | 197,0 443x256, borders 4/11/18/10 | `a` |
| button bank | `dopus_buttons` | empty | 556,0 84x256, borders 0 | `-` |
| backdrop | `DOpus5:DirectoryOpus` (main process) | empty | 0,11 640x245 | `b` |
| small stub | `dopus_lister` | empty | 485,57 80x12, borders 0 | `-` (seen in the first boot's samples only) |

## Telling them apart
- Splash: task `dopus_progressbar` (no other DOpus window has it), title exactly `Directory Opus`, has borders (4/11/4/2), 339x49.
- Button bank: task `dopus_buttons`, empty title, zero borders. Lister: task `dopus_lister`, non-empty path/volume title, borders 4/11/18/10.
- Empty-title windows (buttons, backdrop, stub) cannot be told apart by Title; task name or the `b` flag separates them.
- Patina's PATINA_FOREIGN sentinel prints only title/flags (no task): splash = `Directory Opus/<hex>`, lister = path text, button bank = `-/<hex>`
  (NULL title prints "-"; an empty string prints nothing before the "/"). Note `dk_foreign_windows` skips backdrop windows.
