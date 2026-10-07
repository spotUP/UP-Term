exec {fd}<a.txt; read -u $fd l; echo "$l"; exec {fd}<&-; echo "closed"
