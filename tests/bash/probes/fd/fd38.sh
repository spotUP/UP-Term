exec 3<a.txt 4<b.txt; read -u 3 x; read -u 4 y; echo "$x $y"; exec 3<&- 4<&-; read -u 3 z; echo "st=$?"
