# UP-Term: S:User-Startup with the Assign line of the marked
# ";BEGIN UP-Term assign" block pointed at the drawer in $1, into
# T:User-Startup.up-term; the drawer it named before goes to ENV:upold.
# Run by Install over an Install with a different DEST (reassign, not a second
# block). Only the line inside that block changes; every other line stays byte
# for byte (no field splitting, no backslash processing).
IFS=''
inblk=0
while read -r l; do
    case "$l" in
    ';BEGIN UP-Term assign') inblk=1; printf '%s\n' "$l" ;;
    ';END UP-Term assign') inblk=0; printf '%s\n' "$l" ;;
    'Assign UP-Term: '*)
        if [ "$inblk" = 1 ]; then
            old=${l#*\"}
            old=${old%\"*}
            printf '%s' "$old" >ENV:upold
            printf 'Assign UP-Term: "%s"\n' "$1"
        else
            printf '%s\n' "$l"
        fi ;;
    *) printf '%s\n' "$l" ;;
    esac
done <S:User-Startup >T:User-Startup.up-term
