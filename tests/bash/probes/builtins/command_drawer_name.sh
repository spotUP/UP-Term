# A directory in the current directory named like a command: bash runs the command from $PATH (a
# directory name only cd's under shopt -s autocd, and only when no command is found). vsh entered the
# drawer first (AmigaShell's implicit CD): `man` in a directory holding a drawer man failed.
mkdir sed drawer_q
printf 'abc\n' | sed 's/b/X/'
echo "pipe $?"
sed -n '$=' </dev/null
echo "plain $?"
case $PWD in */sed) echo "entered sed" ;; *) echo "stayed" ;; esac
drawer_q 2>/dev/null
echo "drawer $?"
case $PWD in */drawer_q) echo "entered drawer_q" ;; *) echo "stayed" ;; esac
