exec 3<a.txt; read -u 3 x; echo "got $x"; read -u 3 y; echo "got $y"; exec 3<&-
