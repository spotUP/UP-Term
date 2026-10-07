# a background command that writes to a table handle keeps it open until it ends
exec 3>g
sleep 0.2 >&3 3>f1 & exec 3>&-
wait
echo x >&3 2>/dev/null; echo "rc=$?"
( sleep 0.1; echo y >&3 ) 3>f2 &
echo z >&3 2>/dev/null; echo "rc=$?"
wait
cat f2
