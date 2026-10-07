test -n a -a -n b -o -z c; echo $?; test -z a -o -z b; echo $?; test -n a -a -z b; echo $?; test \( -n a -o -z b \) -a -n c; echo $?
