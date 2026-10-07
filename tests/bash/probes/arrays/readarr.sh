echo "a b c" | { read -a arr; echo ${#arr[@]} ${arr[1]}; }
