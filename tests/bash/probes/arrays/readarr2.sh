echo "a b  c" | { read -a x; declare -p x; }
echo "a:b::c" | { IFS=: read -a x; declare -p x; }
echo "  " | { read -a x; declare -p x; echo $?; }
printf 'a\\ b c\n' | { read -a x; declare -p x; }
printf 'a\\ b c\n' | { read -r -a x; declare -p x; }
printf '1 2 3' | { read -a x; echo $? ${#x[@]}; }
echo "p q" | { x=(1 2 3 4); read -a x; declare -p x; }
echo "k v" | { read -a x y 2>/dev/null; declare -p x; }
