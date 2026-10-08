# compgen -F and -C: COMPREPLY and the command's lines, after the word list (V93)
_f() { COMPREPLY=(zz yy "$2" "$1"); }
compgen -F _f -- ab 2>/dev/null; echo "rc=$?"
compgen -W "a b" -F _f -- a 2>/dev/null
compgen -C "echo a b; echo c" -- x 2>/dev/null; echo "rc=$?"
_e() { COMPREPLY=(); }
compgen -F _e -- x 2>/dev/null; echo "rc=$?"
