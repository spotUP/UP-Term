a=(a b c d); i=1
echo ${a[i]} ${a[i+1]} ${a[-1]} ${a[-2]} ${a[$i]} ${a[${#a[@]}-1]}
echo ${#a[2]} ${#a} ${#a[@]} ${#a[*]}
echo "${a[*]}"; IFS=-; echo "${a[*]}"; IFS=' '
for x in "${a[@]}"; do echo "<$x>"; done
b=("x y" "" z); echo ${#b[@]}; for x in "${b[@]}"; do echo "<$x>"; done; for x in ${b[@]}; do echo "[$x]"; done
set -- "${b[@]}"; echo $#
echo "${e[@]}" ${#e[@]}; e=(); echo ${#e[@]}; echo "${e[@]}x"
c=5; echo ${c[0]} ${c[1]-unset} ${#c[@]}
a[2]=; echo "${a[2]}" ${#a[@]}; echo ${a[9]-none} ${a[9]:-none}
echo $a ${a}
