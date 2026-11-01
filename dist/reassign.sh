# UP-Term: S:User-Startup with the Assign line of the marked
# ";BEGIN UP-Term assign" block pointed at the drawer in $1, into
# T:User-Startup.up-term; the drawer it named before goes to ENV:upold.
# Run by Install over an Install with a different DEST (reassign, not a second
# block). Only that block changes; every other line stays byte
# for byte (no field splitting, no backslash processing).
# The block is written whole, guarded (If EXISTS "<drawer>" around the Assign, so a
# drawer that is gone leaves no assign: BOOT-UPTERM-VOLUME-LOOP), whether it was
# an older unguarded one or a guarded one for another drawer.
IFS=''
inblk=0
while read -r l; do
    case "$l" in
    ';BEGIN UP-Term assign')
        inblk=1
        printf '%s\n' "$l"
        printf 'If EXISTS "%s"\n' "$1"
        printf '  Assign UP-Term: "%s"\n' "$1"
        printf 'EndIf\n' ;;
    ';END UP-Term assign') inblk=0; printf '%s\n' "$l" ;;
    *)
        if [ "$inblk" = 0 ]; then
            printf '%s\n' "$l"
        else
            case "$l" in
            *'Assign UP-Term: '*)
                old=${l#*\"}
                old=${old%\"*}
                if [ "$old" != "$1" ]; then printf '%s' "$old" >ENV:upold; fi ;;
            esac
        fi ;;
    esac
done <S:User-Startup >T:User-Startup.up-term
