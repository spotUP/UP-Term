echo $'a\tb' $'x\ny' $'q\'q' $'\x41\101é' $'\\' $'a\"b'
echo $'' [$'  sp  '] $'\e[0m' | od -c | head -3
x=$'one\ntwo'; echo "$x"; echo ${#x}
echo "$'literal'" '$'"'"'x'"'"
echo $"plain" "$"'x' $"a $x"
case $'a\tb' in $'a\tb') echo match;; esac
echo $'\cA\c?' | od -c | head -2
echo $'\z' $'\x' $'\u' $'\0' | od -c | head -2
printf '%s\n' $'a b' c$'d e'f
