# exec of a missing command: a shell that is not interactive ends with 127, posix mode or not;
# shopt execfail, and a subshell, keep what bash keeps
n() { $BASH -c "$1" 2>/dev/null; echo "st=$?"; }
for s in 'exec /nonexistent/cmd; echo after' 'exec nosuchcmd_zz a b; echo after' \
    'shopt -s execfail; exec nosuchcmd_zz; echo after $?' '(exec nosuchcmd_zz); echo sub $?' \
    'f() { exec nosuchcmd_zz; echo in; }; f; echo after' 'trap "echo bye" EXIT; exec nosuchcmd_zz' \
    'exec nosuchcmd_zz || echo or' 'exec true; echo after'; do
  echo "== $s"
  n "$s"
done
echo "== posix"
$BASH --posix -c 'exec nosuchcmd_zz; echo after' 2>/dev/null; echo "st=$?"
