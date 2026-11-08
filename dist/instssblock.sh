# UP-Term: S:Startup-Sequence with a marked block inserted before the first line that runs a given
# command, into T:Startup-Sequence.up-term (Install copies it back). The one inserter for every
# Startup-Sequence block the kit adds (RemLib before SetPatch, Amarchy-Flat before IPrefs).
#
# Usage: instssblock.sh <name> <before-command> <body line>...
#   <name>            block name; the block is ";BEGIN UP-Term <name>" ... ";END UP-Term <name>", marked like
#                     the S:User-Startup ones, so "unstartup.sh '' S:Startup-Sequence" (Uninstall) and
#                     "unstartup.sh <name> S:Startup-Sequence" strip it again
#   <before-command>  the command the block goes in front of: matched without regard to case, with or
#                     without a "C:" prefix, with or without arguments; the FIRST such line wins
#   <body line>...    one argument per line of the block body
# Writes nothing (and removes any older T: result) when the block is already there or the file has no such line.
IFS=''
file=S:Startup-Sequence
name=$1
cmd=$2
shift 2
# a result left by an earlier run must not be mistaken for this run's (no result file = nothing to do)
rm -f T:Startup-Sequence.up-term
if grep -q "^;BEGIN UP-Term $name\$" "$file"; then exit 0; fi
# the command as a case-insensitive glob: every letter becomes [Xx]; the other characters stay as they are
pairs=AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz
pat=''
rest=$cmd
while [ -n "$rest" ]; do
    c=${rest%"${rest#?}"}
    rest=${rest#?}
    p=$pairs
    hit=0
    while [ -n "$p" ]; do
        two=${p%"${p#??}"}
        p=${p#??}
        if [ "$c" = "${two%?}" ] || [ "$c" = "${two#?}" ]; then pat="$pat[$two]"; hit=1; break; fi
    done
    if [ "$hit" = 0 ]; then pat="$pat$c"; fi
done
found=0
while read -r l; do
    t=${l#"${l%%[! 	]*}"}
    case "$t" in
    $pat|$pat\ *|[Cc]:$pat|[Cc]:$pat\ *)
        if [ "$found" = 0 ]; then
            found=1
            printf '%s\n' ";BEGIN UP-Term $name" "$@" ";END UP-Term $name"
        fi ;;
    esac
    printf '%s\n' "$l"
done <"$file" >T:Startup-Sequence.up-term
if [ "$found" = 0 ]; then rm -f T:Startup-Sequence.up-term; fi
