printf 'ab cd ef' | { read -N 4 x y; echo "[$x][$y]"; }; printf 'ab cd ef' | { read -n 4 x y; echo "[$x][$y]"; }
