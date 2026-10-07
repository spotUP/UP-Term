test -e f.txt 2>/dev/null; echo $?; cd src 2>/dev/null; test -d . ; echo $?; test -f /nonexistent; echo $?; test -s /nonexistent; echo $?
