# an external command's redirections to fds above 2 are undone when it ends (foreground, background, pipeline stage)
cat /dev/null 3>f1
echo a >&3 2>/dev/null; echo "rc=$?"
sleep 0.1 3>f2 &
wait
echo b >&3 2>/dev/null; echo "rc=$?"
echo hi | cat 3>f3 | cat
echo c >&3 2>/dev/null; echo "rc=$?"
exec 3>g
cat /dev/null 3>f4 4>&3
echo d >&3
cat /dev/null 3>f5 &
wait
echo e >&3
cat g
