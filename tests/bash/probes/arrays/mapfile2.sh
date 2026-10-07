printf 'a\nb\n\nc' | { mapfile m; declare -p m; }
printf 'a\nb\n\nc\n' | { mapfile -t m; declare -p m; }
printf 'a\nb\nc\nd\ne\n' | { mapfile -t -n 2 m; declare -p m; }
printf 'a\nb\nc\nd\ne\n' | { mapfile -t -s 3 m; declare -p m; }
printf 'a\nb\n' | { m=(x y z); mapfile -t -O 1 m; declare -p m; }
printf 'a:b:c' | { mapfile -d : -t m; declare -p m; }
printf 'a\nb\n' | { readarray -t; declare -p MAPFILE; }
printf 'x\ny\n' | { mapfile -t arr; echo ${#arr[@]} "${arr[1]}"; }
mapfile -t f < a.txt; echo ${#f[@]}
mapfile -t -n2 g < a.txt; echo ${#g[@]}
