printf 'a,b;c d' | { read -d ';' x y; echo "[$x][$y]$?"; }; printf 'ab' | { read -d , x; echo "[$x]$?"; }
