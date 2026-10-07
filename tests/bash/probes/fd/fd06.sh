exec 3<a.txt; read x <&3; read y <&3; echo "$x|$y"; exec 3<&-
