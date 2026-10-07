echo x
eval 'echo $LINENO
echo $LINENO'
echo $LINENO
f() { eval "echo \$LINENO"; }
f
trap 'echo trap $LINENO' EXIT
