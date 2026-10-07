# a builtin or compound command as the last stage of a pipeline honours its own stdout redirect,
# and the pipeline's stdout (a file, a command substitution) is the stage's stdout
f=${TMPDIR:-/tmp}/pls$$
echo a | { read x; echo "got $x"; } >$f
cat $f
echo b | read y >$f
echo "empty: $(wc -c <$f | tr -d ' ')"
{ echo c | { read x; echo "got $x"; }; } >$f
cat $f
v=$(echo d | { read x; echo "got $x"; })
echo "$v"
echo e | { read x; echo "got $x"; } | cat
echo f | { read x; echo "got $x" >&2; } 2>$f
cat $f
rm -f $f
