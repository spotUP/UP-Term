# reachability P5: fds above 2 resolve through the shell's fd table, {var} allocates
exec 3>r3.txt
echo high >&3
exec 3>&-
exec {fd}<r3.txt
read -u $fd line
exec {fd}<&-
echo "$line"
