# B8 DOPUS-582-FLAT: gate row for Directory Opus 5.82 (Magellan II)

Rig 95, `rig.py start --max` (OS 3.1 master; NOTE `--os 32 --no-dopus` has NO Directory Opus at all, 5.82 is only on the
default master clone when no 5.104 stamp exists). Library: dopus5.library 68.16 (`Version` on the running lib via dopusprobe/STATUS).

## Before (stock gate, build without the row)
    dopus=68.16 inert=unknown sums=05a329c5,aa2c535e      (boot 1, lib at 100f2400; same line on a second DOpus start)

## Load-address check (second boot)
    boot 1: lib=100f2400  sums=05a329c5,aa2c535e
    boot 2: lib=100f8400  sums=05a329c5,aa2c535e    (identical; flat_sum_reloc is address stable)

## LVO -696 / -702 are DrawBox / DrawFieldBox (dopusprobe, 5.82)
    JT drawbox=4ef9103bb404 fieldbox=4ef9103bb4be   (JMP abs.l entries at -696 / -702)
    DUMP0 594f48e7061e26492a4849ee00302f4e00184a40670e206a00043c2800083a280006600c206a00043c2800063a28000870001006...
    DUMP1 594f2f0c49ee00302f2c06702f0a2f092f082f4e00146100000c4fef0010285f584f4e75...
DUMP0: saves registers, tests d0 (recessed) and picks the two pen words from the box in one of two orders (the
swap of the two halves), then the SetAPen/Move/Draw sequence: DrawBox. DUMP1: pushes rp/rect/pens and calls the inner
routine (bsr), pops 16 bytes, rts: DrawFieldBox as a wrapper around the box routine, same shape as 5.104's
(push args, call, return). Not a full disassembly; the runtime proof below (calls rise from 0 to 4 when a lister opens,
only after the patch) is the decisive evidence.

## After (build with the row)
    dopus=68.16 patched=3 calls=0 flat=0 sums=05a329c5,aa2c535e          (DOpus running, before a new lister)
    JT stock  drawbox=4ef9103bb404 fieldbox=4ef9103bb4be
    JT patched drawbox=4ef9105f2f38 fieldbox=4ef9105f2f84
    dopus=68.16 patched=3 calls=4 flat=4 sums=05a329c5,aa2c535e          (after `DeviceList NEW` lister, flat on)
flat=4 of calls=4: every box drawn skipped its bevel (flat_dopus_decide SKIP needs SHINEPEN == BACKGROUNDPEN).

## Tests (make ONLY=flat_core build/vttest_host; ./build/vttest_host flat_core)
    with row:    [OK] flat_core: 2711 checks, 0 failed
    row removed: FAIL tests/test_flat_core.c:248: flat_dopus_gate(flat_dopus_table(&n), n, 68, 16, sums, &why) = 0, want 3
                 FAIL tests/test_flat_core.c:249: why = 1, want 0
                 [FAIL] flat_core: 2711 checks, 3 failed
    Also asserts: 68.15 stays FLAT_GATE_UNKNOWN, 68.16 with other sums stays FLAT_GATE_SUM.
    (Reachability: the gate is exercised on the real amarchy-flat on the rig above, mask 3, calls/flat counters.)

## Screen finding
Directory Opus 5.82 here is the Workbench replacement on the single 800x600 screen titled "Directory Opus"
(listers, the Amarchy bar and Device List are windows of that screen), not a separate own-screen. DrawInfo pen values are
not exposed by the rig tooling; the equality is shown by flat == calls (4 of 4 skipped). A DOpus on its own screen with
its own palette (`rig.py dopus --own-screen` shape) was NOT measured; there the bevel would stay when SHINEPEN != BACKGROUNDPEN
(owner decision, not changed). No screenshot taken (text answers sufficed), none to delete.
