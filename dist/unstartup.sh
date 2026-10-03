# UP-Term: S:User-Startup without the blocks Install added (between the
# ;BEGIN UP-Term and ;END UP-Term lines, and the console and device ones:
# ;BEGIN UP-Term console / device / tmp / serial / wasabi, ;END ...), into T:User-Startup.up-term.
# Uninstall runs it with vsh and copies the result back; every other line
# stays byte for byte (no field splitting, no backslash processing).
IFS=''
skip=0
while read -r l; do
    case "$l" in
    ';BEGIN UP-Term'|';BEGIN UP-Term console'|';BEGIN UP-Term device'|';BEGIN UP-Term tmp'|';BEGIN UP-Term serial'|';BEGIN UP-Term wasabi') skip=1 ;;
    ';END UP-Term'|';END UP-Term console'|';END UP-Term device'|';END UP-Term tmp'|';END UP-Term serial'|';END UP-Term wasabi') skip=0 ;;
    *) if [ "$skip" = 0 ]; then printf '%s\n' "$l"; fi ;;
    esac
done <S:User-Startup >T:User-Startup.up-term
