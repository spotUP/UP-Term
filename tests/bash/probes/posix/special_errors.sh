# posix mode: an error of a special builtin or of an assignment ends a shell that is not interactive
r() { $BASH --posix -c "$1" 2>/dev/null; echo "st=$?"; }
n() { $BASH -c "$1" 2>/dev/null; echo "st=$?"; }
for s in 'set -o nosuch' 'set -Z' 'trap -Z' 'unset -Z' 'readonly -Z' 'readonly Z; unset Z' 'export a-b' \
    'readonly R=1; R=2 :' 'readonly R=1; R=2 echo' 'R=1; readonly R; R=2' ': > /nonexistent/x' '. /nonexistent' \
    'eval "if"' 'eval "("' 'eval "set -o bad"' 'exit abc' 'return 3' 'shift 5' 'break x' 'continue x' \
    'cd /nonexistent' 'echo hi > /nonexistent/x' 'for R in 1; do readonly R; done' 'exec /nonexistent' \
    'f() { eval "if"; echo in; }; f' 'echo $(readonly Z; unset Z; echo sub)' 'f() { return 3; }; f; echo r=$?'; do
  echo "== $s"
  r "$s; echo after"
done
echo "== not posix"
n 'set -o nosuch; echo after'
n 'exit abc; echo after'
