printf 'ab\ncdef' | { read -n 5 x; echo "[$x]"; read -N 3 y; echo "[$y]"; }
