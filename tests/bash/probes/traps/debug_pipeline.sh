trap 'echo "DBG $BASH_COMMAND"' DEBUG
echo a | cat >/dev/null
echo b | cat > /dev/null | cat
x=$(echo c)
(echo d)
{ echo f; } | cat >/dev/null
f() { echo in; }
f | cat >/dev/null
