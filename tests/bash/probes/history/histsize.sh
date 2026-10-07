HISTSIZE=2
history -s q1; history -s q2; history -s q3
history
history -d 2; history
history -d 3 2>/dev/null; echo st=$?
history -c; history -s z; history
