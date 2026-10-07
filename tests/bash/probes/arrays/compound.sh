a=(1 2 3); a+=(4 5); a+=6; echo ${a[@]} ${#a[@]}
b=([2]=x y [7]=z w); echo ${!b[@]}; echo ${b[@]}
c=($(echo p q r) "s t" *.nomatch); echo ${#c[@]} "${c[3]}"
d=(a b); d=("${d[@]}" c); echo ${d[@]}
e=(
  one
  two # no
  three
); echo ${#e[@]}
x=(); x[3]=a; x+=(b); echo ${!x[@]}
declare -a y=(1 "2 3" 4); echo ${#y[@]}
y2=(1 2); y2[1]+=z; echo ${y2[@]}
i=(); declare -i i; i=(1+1 2*3); echo ${i[@]}
n=(1 2); n=7; echo ${n[@]}
s=str; s=(1 2); echo ${s[@]}
q=(a b); q[1]=; echo ${#q[@]}
export z=(1 2) 2>/dev/null; echo "${z[@]}"
readonly ro=(1); ro=(2) 2>/dev/null; echo $?; echo ${ro[@]}
