a=(1 2 3 4); unset 'a[1]'; echo ${!a[@]} ${#a[@]}; unset 'a[-1]'; echo ${a[@]}
unset 'a[@]'; echo ${#a[@]} "${a-gone}"
b=(1 2); unset b; echo "${b-gone}" ${#b[@]}
x=5; unset 'x[0]'; echo "${x-gone}"
c=(1 2 3); unset c[1]; c[1]=n; echo "${c[@]}"
d=(1 2 3); i=2; unset "d[$i]"; echo "${d[@]}"
readonly r=(1 2); unset 'r[0]' 2>/dev/null; echo $? "${r[@]}"
