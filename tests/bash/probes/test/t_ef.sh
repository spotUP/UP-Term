echo a > e1; ln e1 e2; echo a > e3; [ e1 -ef e2 ]; echo $?; [ e1 -ef e3 ]; echo $?; [ e1 -ef e1 ]; echo $?
