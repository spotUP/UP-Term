exec 4<a.txt; read x <&4; echo $x; exec 4<&-
