#!/bin/sh
# DV5 of the console.device plan: the device checks on one Kickstart.
#   tools/rig/dvmatrix.sh <kickstart file> <label>
# Boots the rig on that ROM and runs condev (DV1), devverify (DV2, DV4),
# rkc (D2.3), cudump (D3.2), concon (H5.6); one summary line per check into
# build/rig/shots/dvmatrix-<label>.log. Stops the rig at the end.
set -u
cd "$(dirname "$0")/../.."
K="$1"; L="$2"; LOG=build/rig/shots/dvmatrix-$L.log
python3 tools/rig/rig.py stop >/dev/null
python3 tools/rig/rig.py start --kick "$K" | tail -1
: > "$LOG"
for t in condev_rig devverify_rig rkc_rig cudump_rig concon_rig; do
    out=$(timeout 900 python3 -u tools/rig/$t.py 2>&1)
    summary=$(printf '%s\n' "$out" | grep -E "^passed|^differ:" | tail -1)
    fails=$(printf '%s\n' "$out" | grep -c "^FAIL")
    echo "$L $t: $summary (FAIL lines: $fails)" | tee -a "$LOG"
done
python3 tools/rig/rig.py stop >/dev/null
