# wasabid PR 8 (37f387f) screens test, OS 3.1 fs-uae rig 52

PR build md5 (host, built from creep-ltx/wasabi 37f387f): 27f53fe0b351f058dfe845cfa0f058ac, size 115764; pinned b9d1812 build md5: 217b3d9202021186f51cec4612433b91
Rig 52 boot daemon port 1285 = pinned wasabid (BOOTX:wasabid). PR daemon: put to RAM:wasabid-pr (115764 bytes, same size as host file), run as 'RAM:wasabid-pr 1290 name pr' on port 1290; Status lists Process RAM:wasabid-pr.

```
$ wasabi --port 1290 screen   (run 1)
ADDR       SIZE        DEPTH  TITLE
0x10010f38 800x600        24  Directory Opus  2059344 graphics mem  1071691848 other mem   <- front
rc=0
$ wasabi --port 1290 screen   (run 2)
ADDR       SIZE        DEPTH  TITLE
0x10010f38 800x600        24  Directory Opus  2059344 graphics mem  1071691848 other mem   <- front
rc=0
$ wasabi --port 1290 screen   (run 3)
ADDR       SIZE        DEPTH  TITLE
0x10010f38 800x600        24  Directory Opus  2059344 graphics mem  1071691848 other mem   <- front
rc=0
$ wasabi --port 1290 screen   (run 4)
ADDR       SIZE        DEPTH  TITLE
0x10010f38 800x600        24  Directory Opus  2059344 graphics mem  1071691848 other mem   <- front
rc=0
$ wasabi --port 1290 screen   (run 5)
ADDR       SIZE        DEPTH  TITLE
0x10010f38 800x600        24  Directory Opus  2059344 graphics mem  1071691848 other mem   <- front
rc=0
$ wasabi --port 1290 windows
screen "Directory Opus  2059344 graphics mem  1071691848 other mem"  800x600, 24 bit
  window "Amarchy bar"  at 0,587  800x13  task UP-Term:bin/Amarchy
  window ""  at 12,456  39x15  task dopus_start_menu
  window ""  at 0,14  800x573  task DOpus5:DirectoryOpus  [active, backdrop]
rc=0
$ wasabi --port 1285 screen   (pinned daemon, comparison)
ADDR       SIZE        DEPTH  TITLE
0x10010f38 800x600        24  Directory Opus  2059344 graphics mem  1071691848 other mem   <- front
rc=0
```

Verdict: PASS on a single-screen desktop (Directory Opus 800x600). Second-screen run not done.
