# RL11 follow-up: network volume icon and disk icons after a restart

Commit fe2405a7 (branch worktree-agent-a32d9f57a7b8e2912), not pushed.

## Implemented
- dist/lucide-icons: `type:net = globe-hemisphere-west`; dist/lucide-icons-outline: `type:net = network`. `make lucide-dat` OK (51 / 52 icons).
- amarchy/iconapply_core.c: `ia_vol_is_net(devname, handler)` is pure: case-insensitive substrings smb, samba, cifs, nfs, ftpmount, netfs, webdav, sftp in the DosList device name or its handler file. `ia_walk_net` applies `type:net` to `<vol>Disk.info` (a missing file is created from def_disk and recorded absent, so RESTORE removes it) and counts `c->net`.
- amarchy/icons.c: walk_volumes finds the device entry sharing the volume's MsgPort (vol_is_net) and calls ia_walk_net after walk_dir (the walk's def_disk fallback would otherwise overwrite it). Summary line gains `net=N`.

## Tests
- Host: `make test ONLY=iconapply`: 166 checks, 0 failed (new test_net: classifier cases, creation, sentinel, manifest absent, RESTORE removes).
- tests/test_icons_skip_glue.py: new pin; fails with the ia_walk_net call removed, passes restored.
- `make amarchy-icons`: clean (warnings as errors).

## Rig 77 (FS-UAE, OS 3.2)
No network volume mounted: net=0 in the APPLY summary; the classifier is proven on host only.
APPLY: written=185 fallback=126 net=0. Rebooted after 5 s quiet (the reboot is the Workbench restart), screenshot.
Pixel measure (PIL, icon cells x20..75, y=30+36k, five volumes; non-background pixels):
- before (grey palette): (122,122,122) (141,141,141) (190,190,190) (0,0,0), plus (153,51,102) on one.
- after (flat): (169,177,214) 146..216 px, (134,141,177), (99,107,140), (0,0,0); no grey left. The four def_disk volumes have identical colours.
Caveat: the reboot also reset the screen (background 34,34,34 -> 170,170,170), so before/after differ by more than the icons; the icon cells were compared by position.
Shots: ~/Desktop/upterm-shots/s31-rl11-disk-before.png, s31-rl11-disk-after.png.

## Open
- A real network volume is untested on a rig (no smbfs handler). First live proof needs an smb2fs mount.
- Each APPLY on a net volume re-applies def_disk then type:net (two writes of one file); the end state is stable.
