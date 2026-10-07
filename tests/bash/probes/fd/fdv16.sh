# a background or pipeline-stage subshell or group with an fd redirection leaves the parent's table alone
exec 3>g
( echo a >&3 ) 3>f1 &
wait
echo b >&3
( sleep 0.2; echo c >&3 ) 3>f2 &
echo d >&3
wait
{ echo e >&3; } 3>f3 &
wait
echo f >&3
( echo h >&3 ) 3>f4 | cat
echo i >&3
{ echo j >&3; } 3>f5 | cat
echo k >&3
cat f1 f2 f3 f4 f5 | sort
cat g
