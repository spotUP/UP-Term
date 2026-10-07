# the fd-handling lines of an autoconf 2.72 configure prologue
as_me=configure
exec 5>>config.log
{
  echo
  echo "## --------- ##"
  echo "## Platform. ##"
  echo "## --------- ##"
  echo
} >&5
exec 6>&1
test -n "$DJDIR" || exec 7<&0 </dev/null
as_fn_error () {
  as_status=$1; test $as_status -eq 0 && as_status=1
  if test "$4"; then
    as_lineno=${as_lineno-"$3"} as_lineno_stack=as_lineno_stack=$as_lineno_stack
    printf '%s\n' "$as_me:${as_lineno-$LINENO}: error: $2" >&$4
  fi
  printf '%s\n' "$as_me: error: $2" >&2
}
ac_try='echo compiling'
(eval "$ac_try") 2>&5
printf '%s\n' "$as_me:$LINENO: $ac_try" >&5
printf %s "checking for gcc... " >&6
printf '%s\n' "gcc" >&6
printf '%s\n' "yes" >&6
ac_status=3
(exit $ac_status) && printf '%s\n' "$as_me:$LINENO: \$? = $ac_status" >&5
as_fn_error 1 "no compiler" 5 5 2>/dev/null
test -t 7; echo "tty7=$?"
cat <&7 | wc -l | tr -d ' '
exec 7<&-
AS_MESSAGE_FD=6
exec {as_fd}>&$AS_MESSAGE_FD
printf '%s\n' "via the saved fd" >&$as_fd
exec {as_fd}>&-
exec 6>&- 5>&-
echo "log:"; cat config.log
