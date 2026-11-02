# wasabid cfdd89d1 screen listing with a second public screen, OS 3.1 fs-uae rig 56

Pinned wasabid (tools/nas/wasabi.rev, cfdd89d1), build/wasabi/wasabid md5 27f53fe0b351f058dfe845cfa0f058ac; rig boot copy build/rig56/boot/wasabid has the same md5. Rig started: UPTERM_RIG=56 rig.py start --max --purpose "vtcon-05 s50: wasabi screens second-screen test". Boot daemon port 1289, rig key from tools/rig/paths.py WASABI_KEY. Second screen: build/amiga/scrprobe put to RAM:scrprobe, 'Run >NIL: RAM:scrprobe open G7pub 240' (public screen "G7 screen", the method of tools/rig/amarchy_gui_rig.py case_screen). Client: python3 build/wasabi/wasabi --host 127.0.0.1 --port 1289 --key <rig key> screen.

```
-- run 1
ADDR       SIZE        DEPTH  TITLE
0x10109618 640x256         3  G7 screen   <- front
0x1010b910 800x600        24  Directory Opus  1997168 graphics mem  1071953016 other mem
rc=0
-- run 2
ADDR       SIZE        DEPTH  TITLE
0x10109618 640x256         3  G7 screen   <- front
0x1010b910 800x600        24  Directory Opus  1997168 graphics mem  1071953016 other mem
rc=0
-- run 3
ADDR       SIZE        DEPTH  TITLE
0x10109618 640x256         3  G7 screen   <- front
0x1010b910 800x600        24  Directory Opus  1997168 graphics mem  1071953016 other mem
rc=0
-- run 4
ADDR       SIZE        DEPTH  TITLE
0x10109618 640x256         3  G7 screen   <- front
0x1010b910 800x600        24  Directory Opus  1997168 graphics mem  1071953016 other mem
rc=0
-- run 5
ADDR       SIZE        DEPTH  TITLE
0x10109618 640x256         3  G7 screen   <- front
0x1010b910 800x600        24  Directory Opus  1997168 graphics mem  1071953016 other mem
rc=0
-- windows
screen "G7 screen"  640x256, 3 bit, rows x2 as in grab
screen "Directory Opus  1997168 graphics mem  1071953016 other mem"  800x600, 24 bit
  window "Amarchy bar"  at 0,587  800x13  task UP-Term:bin/Amarchy
  window ""  at 12,456  39x15  task dopus_start_menu
  window ""  at 0,14  800x573  task DOpus5:DirectoryOpus  [active, backdrop]
rc=0
```

Verdict: PASS. 5 of 5 runs rc=0, both screens listed every time (G7 screen 640x256 front, Directory Opus 800x600), windows rc=0 lists both screens. Rig 56 stopped and cleared after the last command plus 5 s.
