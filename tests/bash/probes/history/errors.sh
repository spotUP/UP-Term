history -x; echo st=$?
history abc; echo st=$?
history -s; echo st=$?
history -c -s q; history
history -rw 2>/dev/null; echo st=$?
