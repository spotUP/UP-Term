exec 3<a.txt; [ -r /dev/fd/3 ]; echo $?; exec 3>&-; exec 4>o.txt; [ -w /dev/fd/4 ]; echo $?; exec 4>&-; [ -r /dev/fd/4 ]; echo $?
