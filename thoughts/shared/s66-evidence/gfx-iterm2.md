
## O4/O5 finish

Branch gfx-iterm2, second commit on top of 1ad3c436 (O4, O5, O10).

- inline=0 files: engine/kitty.c iterm_end hands the decoded bytes to the host through
  `vt_set_download(t, writer, host)` (engine/vtengine.h). Path
  `ENVARC:UP-Term/Downloads/<host>/<name>`; name = base name of the base64 `name=` value
  (kept in `t->iname`, since `str` skips it), no `/`, `\`, `:`, control chars; "." / ".." / empty
  become "download"; host sanitised the same. Cap 4 MB = KIT_RAW_MAX (no setting): over it the file
  is dropped and `cb.notify("iTerm2", "download over 4 MB dropped")` fires (no log callback exists).
  No writer set: dropped, as before.
- ReportCellSize: `OSC 1337;ReportCellSize ST` answers `OSC 1337;ReportCellSize=<cell_h>;<cell_w> ST`
  (8x16 when the host never set cell pixels). Capabilities: no reply. Note: real iTerm2 adds a third
  field (scale); the owner format has two.
- Tests: ONLY=iterm2 85 checks 0 failed; ONLY=kitty 470 checks 0 failed. vbcc vtengine.c 68000 and
  68020 (-DVT_AMIGA_EXEC_ALLOC, -warnings-as-errors) clean.
- Mutations (each failed the test, then restored; final run green):
  - cap check `if (k->full)` -> `if (0)`: 2 fails (cap test sv_calls)
  - base-name cut `start = i + 1` -> `start = 0`: sanitising + save tests fail
  - `"ReportCellSize"` match broken: reply test fails
  - writer call replaced by a stub: save test fails
  - host sanitising removed: save path test fails
- Open: the UP-Term handler does not yet call vt_set_download (needs a writer that creates
  ENVARC:UP-Term/Downloads/<host>/ and writes the file; shared with RIP-7b). Rig check IT-4 and
  IT-0 cursor rule unchanged from the first commit.
