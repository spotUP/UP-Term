trap 'echo "ERR $? line" ' ERR
false
echo a
true && false
echo b
false || true
if false; then :; fi
! false
x=$(false); echo c
f() { false; echo inf; }
f
echo d
{ false; }
( false )
false | true
true | false
set -E
f
( false; echo sub )
g() { return 3; }
g
echo $(false)
trap - ERR
false
trap 'echo E2' ERR
trap -p ERR
trap
