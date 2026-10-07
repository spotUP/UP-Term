exec 3<a.txt; read x </dev/fd/3; echo "$x"; exec 3<&-
