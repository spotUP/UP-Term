f() { echo in-f; }
trap 'echo "RET $FUNCNAME"' RETURN
f
g() { f; echo in-g; }
g
echo main
trap - RETURN
h() { trap 'echo hret' RETURN; echo in-h; }
h
echo end
. /dev/null
