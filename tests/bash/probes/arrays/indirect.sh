a=(p q r); n=a; echo ${!n}; m='a[1]'; echo ${!m}
x=hello; y=x; echo ${!y} ${!y:-d}
b=(1 2 3); echo "${!b[@]}"; echo ${!b[*]}
declare -A h=([k]=v); echo "${!h[@]}"
echo "${!e[@]}x"
