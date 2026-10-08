# $_ at startup is the shell's own path, before any command ran (V42)
case $_ in
*/bash|*/vsh_host|*vsh) echo "the shell" ;;
*) echo "[$_]" ;;
esac
