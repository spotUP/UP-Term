# UP-Term: S:User-Startup without the blocks Install added, into
# T:User-Startup.up-term. A block runs from ";BEGIN UP-Term" or
# ";BEGIN UP-Term <name>" to the matching ";END UP-Term[ <name>]". Any name
# matches, not a fixed list: an older copy of this script in ENVARC:up-term
# must still remove the blocks a newer kit wrote (the Replay kept
# ";BEGIN UP-Term python" after an Uninstall whose script predated it).
# Uninstall runs it with vsh and copies the result back; every other line
# stays byte for byte (no field splitting, no backslash processing).
IFS=''
skip=0
while read -r l; do
    case "$l" in
    ';BEGIN UP-Term'|';BEGIN UP-Term '*) skip=1 ;;
    ';END UP-Term'|';END UP-Term '*) skip=0 ;;
    *) if [ "$skip" = 0 ]; then printf '%s\n' "$l"; fi ;;
    esac
done <S:User-Startup >T:User-Startup.up-term
