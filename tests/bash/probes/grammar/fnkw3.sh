function a { echo a; }; function b() { echo b; }; function c () { echo c; }; function d
{ echo d; }
a; b; c; d
e () { echo e; }; e ( ) { echo e2; } 2>/dev/null; e
function f { echo "f $1"; } ; f 1; f2 ( ) ( echo sub ); f2
function g { local x=1; echo $x; }; g; type g | head -1
function h { return 3; }; h; echo rc=$?
function while_ { echo ok; }; while_
