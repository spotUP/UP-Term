printf 'x y\n' | { read -t 1 a b; echo "$? $a $b"; }; printf '' | { read -t 1 a; echo "$?"; }
