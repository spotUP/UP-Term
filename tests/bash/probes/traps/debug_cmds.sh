trap 'echo "DBG $BASH_COMMAND"' DEBUG
echo a
x=1
f() { echo inf; }
f
[[ a == a ]]
(( 1 ))
for i in 1 2; do echo $i; done
case a in a) echo c;; esac
if true; then echo t; fi
true && echo and
trap - DEBUG
echo off
