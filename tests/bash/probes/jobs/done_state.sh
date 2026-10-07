exec 2>/dev/null
sleep 0 &
wait $!
jobs
sleep 0.2 & 
true &
sleep 0.4
jobs
jobs
echo "rc=$?"
sleep 5 &
x=$(jobs -p)
kill $x
wait
