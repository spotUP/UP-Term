la=(9 9); f() { local -a la=(1 2); la[3]=x; }
a=(1 2 3); declare -a g=(7 8); g=(1 2 3)
f; a[1]=b; b[2]=c
echo "${a[@]}" "${g[@]}" "${#b[@]}"
