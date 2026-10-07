printf 'ab\0cd\0' | { read -d '' x; echo "[$x]$?"; read -d '' x; echo "[$x]$?"; }
