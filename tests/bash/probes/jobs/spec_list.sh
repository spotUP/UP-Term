exec 2>/dev/null
set -m
sleep 2 &
sleep 2 &
sleep 2 &
jobs | sed "s/[0-9][0-9][0-9][0-9]*/PID/" | sed -n l
jobs -l | sed 's/[0-9]\{3,\}/PID/' | sed "s/[0-9][0-9][0-9][0-9]*/PID/" | sed -n l
jobs -p | sed 's/[0-9]\{3,\}/PID/'
jobs -r | sed "s/[0-9][0-9][0-9][0-9]*/PID/" | sed -n l
jobs -s | sed "s/[0-9][0-9][0-9][0-9]*/PID/" | sed -n l
jobs %2 | sed "s/[0-9][0-9][0-9][0-9]*/PID/" | sed -n l
jobs %- %+ | sed "s/[0-9][0-9][0-9][0-9]*/PID/" | sed -n l
jobs %sl; echo rc=$?
jobs %?7 | sed "s/[0-9][0-9][0-9][0-9]*/PID/" | sed -n l
jobs %9; echo rc=$?
jobs -x echo hi %1 | sed "s/[0-9][0-9][0-9][0-9]*/PID/"
jobs -z; echo rc=$?
disown %1
jobs | sed "s/[0-9][0-9][0-9][0-9]*/PID/" | sed -n l
disown -h %2; jobs | sed "s/[0-9][0-9][0-9][0-9]*/PID/" | sed -n l
disown -a; jobs; echo rc=$?
disown %5; echo rc=$?
sleep 0 &
p=$!
wait $p; echo rc=$?
sleep 1 & sleep 0 &
wait -n; echo rc=$?
wait -p v -n; echo "v set: ${v:+yes}"
wait; echo rc=$?
( exit 3 ) &
wait $!; echo rc=$?
( exit 4 ) &
wait %1; echo rc=$?
wait 99999; echo rc=$?
wait %9; echo rc=$?
true &
true &
wait %+ ; echo rc=$?
