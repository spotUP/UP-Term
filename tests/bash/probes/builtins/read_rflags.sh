printf 'a\\b c\n' | { read -r x y; echo "$x|$y"; }; printf 'a\\b c\n' | { read x y; echo "$x|$y"; }; printf 'a b\n' | { read -rs x; echo $x; }
