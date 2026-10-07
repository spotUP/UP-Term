# posix mode: the assignments before a special builtin stay; before anything else they do not
r() { $BASH --posix -c "$1" 2>/dev/null; echo "st=$?"; }
r 'FOO=1 :; echo "[$FOO]"'
r 'FOO=1 export BAR=2; echo "[$FOO][$BAR]"'
r 'f() { FOO=1 eval :; }; f; echo "[$FOO]"'
r 'FOO=1 set -- a; echo "[$FOO][$1]"'
r 'FOO=1 command :; echo "[$FOO]"'
r 'FOO=1 true; echo "[$FOO]"'
r 'f() { :; }; FOO=1 f; echo "[$FOO]"'
$BASH -c 'FOO=1 :; echo "[$FOO]"'; echo "st=$?"
