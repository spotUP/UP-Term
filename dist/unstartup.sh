# UP-Term: S:User-Startup without the block Install added (between the
# ;BEGIN UP-Term and ;END UP-Term lines), into T:User-Startup.up-term.
# Uninstall runs it with vsh and copies the result back; every other line
# stays byte for byte (no field splitting, no backslash processing).
IFS=''
skip=0
while read -r l; do
    case "$l" in
    ';BEGIN UP-Term') skip=1 ;;
    ';END UP-Term') skip=0 ;;
    *) if [ "$skip" = 0 ]; then printf '%s\n' "$l"; fi ;;
    esac
done <S:User-Startup >T:User-Startup.up-term
