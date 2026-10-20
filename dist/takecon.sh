# UP-Term: S:User-Startup with each line that mounts another console
# replacement on CON: or RAW: ("Mount CON: from DEVS:KingCON-mountlist", the
# A1200's KingCON) turned off, into T:User-Startup.up-term. A line turned off
# becomes ";UP-Term off: <the line>"; unstartup.sh (Uninstall) turns it back
# on. UPConsole refuses CON ON while another replacement serves CON: (DD21:
# never stack), so Install turns its Mount lines off and UP-Term serves CON:
# from the next boot. T:UPTCONOFF lists every line that is off, those an
# earlier Install turned off too; none, no T:UPTCONOFF. Every other line stays
# byte for byte (no field splitting, no backslash processing).
IFS=''
while read -r l; do
    off=
    case "$l" in
    ';UP-Term off: '*)
        printf '%s\n' "${l#;UP-Term off: }" >>T:UPTCONOFF ;;
    [Mm][Oo][Uu][Nn][Tt][' 	']*|[Cc]:[Mm][Oo][Uu][Nn][Tt][' 	']*)
        case "$l" in
        *[' 	'][Cc][Oo][Nn]:*|*[' 	'][Rr][Aa][Ww]:*) off=1 ;;
        esac ;;
    esac
    if [ -n "$off" ]; then
        printf '%s\n' "$l" >>T:UPTCONOFF
        printf ';UP-Term off: %s\n' "$l"
    else
        printf '%s\n' "$l"
    fi
done <S:User-Startup >T:User-Startup.up-term
