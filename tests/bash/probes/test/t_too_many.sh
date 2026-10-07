test a = a -a b = b; echo $?; test a = a -a b = c; echo $?; test a = b -o b = b; echo $?; [ a = a -a -n "" -o x = x ]; echo $?
