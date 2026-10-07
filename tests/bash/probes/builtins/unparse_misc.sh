f() { echo 1; }
g() { echo 2; }
a() { cat <<EOF
x
EOF
}
declare -f
echo ---
declare -F
echo ---
declare -f nosuch; echo $?
type g
sleep 0 &
{ echo a; echo b; } &
wait
trap 'echo "[$BASH_COMMAND]"' DEBUG
cat <<EOF >/dev/null
d
EOF
[[ -n x && -n y ]]
(( 1 + 2 ))
trap - DEBUG
