# $$ is the top shell's pid in a subshell, a command substitution, a pipeline stage and a loop body there;
# BASHPID is each one's own (configure writes conf$$subs.awk in one place and reads it in a `...` )
top=$$
[ "`echo $$`" = "$top" ] && echo backtick
[ "$(echo $$)" = "$top" ] && echo dollar-paren
( [ "$$" = "$top" ] && echo subshell )
{ echo $$; } | { read p; [ "$p" = "$top" ] && echo pipeline; }
for i in 1 2; do x=`echo "$$" | sed 's/^/a/'`; [ "$x" = "a$top" ] && echo loop$i; done
[ "$(echo $BASHPID)" != "$BASHPID" ] && echo bashpid-differs
