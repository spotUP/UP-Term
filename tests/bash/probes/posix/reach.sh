FOO=1 :
echo "[$FOO]"
export X=1
export -p | grep " X="
type : cd
set +e
y=$(true; echo hi)
echo "[$y]"
POSIXLY_CORRECT=
set +o posix
set -o posix
echo "[$POSIXLY_CORRECT]"
set -o nosuch
echo not reached
