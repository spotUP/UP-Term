# UP-Term: S:User-Startup without the blocks Install added, into
# T:User-Startup.up-term. A block runs from ";BEGIN UP-Term" or
# ";BEGIN UP-Term <name>" to the matching ";END UP-Term[ <name>]". Any name
# matches, not a fixed list: an older copy of this script in ENVARC:up-term
# must still remove the blocks a newer kit wrote (the Replay kept
# ";BEGIN UP-Term python" after an Uninstall whose script predated it).
# With a block name as the first argument it removes only that block (an
# add-on archive's Uninstall takes out its own: "unstartup.sh python" for
# ";BEGIN UP-Term python"; a stand-alone archive's block carries no UP-Term in
# its name, ";BEGIN Python3", and goes by "unstartup.sh Python3"); with none,
# every ";BEGIN UP-Term" block (the base's Uninstall, and an older kit's copy;
# a stand-alone archive's block is not touched by it).
# Uninstall runs it with vsh and copies the result back; every other line
# stays byte for byte (no field splitting, no backslash processing).
IFS=''
name=$1
skip=0
while read -r l; do
    if [ -n "$name" ]; then
        case "$l" in
        ";BEGIN UP-Term $name"|";BEGIN $name") skip=1 ;;
        ";END UP-Term $name"|";END $name") skip=0 ;;
        *) if [ "$skip" = 0 ]; then printf '%s\n' "$l"; fi ;;
        esac
    else
        case "$l" in
        ';BEGIN UP-Term'|';BEGIN UP-Term '*) skip=1 ;;
        ';END UP-Term'|';END UP-Term '*) skip=0 ;;
        *) if [ "$skip" = 0 ]; then printf '%s\n' "$l"; fi ;;
        esac
    fi
done <S:User-Startup >T:User-Startup.up-term
