echo one
sleep 0 &
wait
echo $!x >/dev/null
echo ${#!}
