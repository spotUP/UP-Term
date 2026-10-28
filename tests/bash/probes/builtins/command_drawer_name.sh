# A directory in the current directory named like a command: bash runs the command from $PATH (a
# directory name only cd's under shopt -s autocd, and only when no command is found). vsh entered the
# drawer first (AmigaShell's implicit CD): `man` in a directory holding a drawer man failed.
# A drawer named like no command is a cd in vsh on purpose: probe builtins/autocd_bare_drawer.
mkdir sed
printf 'abc\n' | sed 's/b/X/'
echo "pipe $?"
sed -n '$=' </dev/null
echo "plain $?"
case $PWD in */sed) echo "entered sed" ;; *) echo "stayed" ;; esac
