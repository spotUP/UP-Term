[ ! a = a ]; echo $?; [ ! -n x ]; echo $?; [ \( -n x \) ]; echo $?; [ -n x -a -n y ]; echo $?; [ -z x -o -n y ]; echo $?
