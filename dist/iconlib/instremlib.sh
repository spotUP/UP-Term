# UP-Term: S:Startup-Sequence with a RemLib block inserted before the SetPatch line, into
# T:Startup-Sequence.up-term (Install copies it back). KS 3.0/3.1 keep their ROM icon.library
# resident, which wins over LIBS:icon.library; RemLib removes it from the resident list, and
# that has to happen BEFORE SetPatch (IconLib.html, "RemLib"). The block is marked like the
# S:User-Startup ones, so "unstartup.sh '' S:Startup-Sequence" (Uninstall) strips it again.
# Writes nothing when the block is already there or the file has no SetPatch line.
IFS=''
file=S:Startup-Sequence
if grep -q '^;BEGIN UP-Term RemLib$' "$file"; then exit 0; fi
found=0
while read -r l; do
    t=${l#"${l%%[! 	]*}"}
    case "$t" in
    [Cc]:[Ss][Ee][Tt][Pp][Aa][Tt][Cc][Hh]|[Cc]:[Ss][Ee][Tt][Pp][Aa][Tt][Cc][Hh]\ *|[Ss][Ee][Tt][Pp][Aa][Tt][Cc][Hh]|[Ss][Ee][Tt][Pp][Aa][Tt][Cc][Hh]\ *)
        if [ "$found" = 0 ]; then
            found=1
            printf '%s\n' ';BEGIN UP-Term RemLib' 'If EXISTS LIBS:icon.library' '   RemLib >NIL: icon.library' 'EndIf' ';END UP-Term RemLib'
        fi ;;
    esac
    printf '%s\n' "$l"
done <"$file" >T:Startup-Sequence.up-term
if [ "$found" = 0 ]; then rm -f T:Startup-Sequence.up-term; fi
