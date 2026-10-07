history -s a; history -s b; history -s c; history -s d
history -d 1; history
history -d -1; history
history -d 9; echo st=$?
history -d 0; echo st=$?
history -d x; echo st=$?
history -c; history -s only; history
