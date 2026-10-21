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
# a stand-alone archive's block is not touched by it). With none it also
# turns back on the lines takecon.sh turned off (";UP-Term off: <line>", the
# Mount lines of another console replacement).
# A second argument names another file to strip instead of S:User-Startup (Install's
# upkg Alias lives in S:Shell-Startup: "unstartup.sh '' S:Shell-Startup"), the result
# in T:<file without S:>.up-term (T:Shell-Startup.up-term); the first argument is
# then "" for every UP-Term block or a block name.
# Uninstall runs it with vsh and copies the result back; every other line
# stays byte for byte (no field splitting, no backslash processing).
IFS=''
name=$1
file=$2
if [ -z "$file" ]; then file=S:User-Startup; fi
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
        ';UP-Term off: '*) if [ "$skip" = 0 ]; then printf '%s\n' "${l#;UP-Term off: }"; fi ;;
        *) if [ "$skip" = 0 ]; then printf '%s\n' "$l"; fi ;;
        esac
    fi
done <"$file" >"T:${file#S:}.up-term"
