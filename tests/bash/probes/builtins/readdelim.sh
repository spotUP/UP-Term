printf 'a:b' | { read -d : x; echo $x; }
